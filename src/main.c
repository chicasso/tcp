#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>

#define PORT 4221
#define CRLF_SIZE 2
#define MAX_BUFFER 4024
#define MAX_HEADERS 1024

struct PathDetails {
  int present;
  char* param;
  char* path;
  int paramSize;
};

struct HeaderDetails {
  int present;
  char* value;
  char* name;
  int size;
};

enum HttpMethod {
  GET = 0,
  POST = 1,
  PUT = 2,
  DELETE = 3,
};

struct Request {
  char * __method;
  char * __endpoint;
  char * __param;
  char ** __headers;
  char * __body;
  int __max_size;
  char * __request;
  int __header_count;
  char * __base_endpoint;
  int __base_endpoint_exists;
};

void parse_request(char* request_buff, struct Request * req, char * base_endpoint) {
  req->__max_size = MAX_BUFFER;
  req->__request = request_buff;
  req->__base_endpoint = NULL;
  req->__base_endpoint_exists = 1;
  req->__param = NULL; // Initialize param to NULL

  if (base_endpoint == NULL) {
    base_endpoint = "/";
  }
  
  /* SETTING METHOD */
  char * status_line_start = request_buff;
  char * http_method_line_end = strstr(request_buff, " /");

  req->__method = (char *)malloc(http_method_line_end - status_line_start + 1); // +1 for null terminator
  strncpy(req->__method, status_line_start, http_method_line_end - status_line_start);
  req->__method[http_method_line_end - status_line_start] = '\0'; // Add null terminator
  
  char * status_line_end = strstr(request_buff, "HTTP/1.1\r\n");

  /* SETTING ENDPOINT */
  req->__endpoint = (char *)malloc(status_line_end - 1 - (http_method_line_end + 1) + 1); // +1 for null terminator
  strncpy(req->__endpoint, http_method_line_end + 1, status_line_end - 1 - (http_method_line_end + 1));
  req->__endpoint[status_line_end - 1 - (http_method_line_end + 1)] = '\0'; // Add null terminator
  
  /* SETTING PARAM */
  char * param_start = strstr(req->__endpoint, base_endpoint);

  if (param_start != NULL && param_start == req->__endpoint) { // Check if base_endpoint is at the start
    req->__base_endpoint = strdup(base_endpoint);

    if (strlen(req->__endpoint) > strlen(base_endpoint)) {
      param_start += strlen(base_endpoint);
      
      // Only extract param if there's something after the base_endpoint
      if (*param_start == '/') {
        param_start++; // Skip the leading '/'
        
        // Check if the param contains any additional path segments
        if (strchr(param_start, '/') != NULL) {
          // Multiple segments after the base_endpoint (e.g., "/user-agent/user1/user2")
          req->__base_endpoint_exists = 0;
          req->__param = (char *)malloc(strlen(param_start) + 1);
          strcpy(req->__param, param_start);
        } else {
          // Only one segment after the base_endpoint (e.g., "/user-agent/user1")
          req->__param = (char *)malloc(strlen(param_start) + 1);
          strcpy(req->__param, param_start);
        }
      } else {
        req->__param = (char *)malloc(1);
        req->__param[0] = '\0';
      }
    } else {
      // Exact match, empty param
      req->__param = (char *)malloc(1);
      req->__param[0] = '\0';
    }
  } else {
    req->__base_endpoint_exists = 0;
  }

  /* SETTING HEADERS */
  char * header_start = strstr(req->__request, "\r\n");
  req->__headers = (char **) malloc(MAX_HEADERS * sizeof(char *));

  if (header_start != NULL) {
    header_start += 2;

    char * header_end = strstr(req->__request, "\r\n\r\n");
    if (header_end != NULL) {
      char * header_pointer = header_start;
      req->__header_count = 0;  

      while (header_pointer < header_end) {
        char * header_line_end = strstr(header_pointer, "\r\n");
        if (!header_line_end || header_line_end > header_end) {
          break;
        }

        int header_length = header_line_end - header_pointer;
        req->__headers[req->__header_count] = malloc(header_length + 1);

        strncpy(req->__headers[req->__header_count], header_pointer, header_length);
        req->__headers[req->__header_count][header_length] = '\0';
        
        req->__header_count += 1;
        header_pointer = header_line_end + 2;
      }
    } else {
      free(req->__headers);
      req->__headers = NULL;
      req->__header_count = 0;
    }
  } else {
    free(req->__headers);
    req->__headers = NULL;
    req->__header_count = 0;
  }

  /* SETTING BODY */
  char * body_start = strstr(request_buff, "\r\n\r\n");
  
  if (body_start != NULL) {
    body_start += 4; // Skip past \r\n\r\n
    char * body_end = request_buff + strlen(request_buff);
    int body_length = body_end - body_start;

    if (body_length > 0) {
      req->__body = (char *) malloc(body_length + 1);
      strncpy(req->__body, body_start, body_length);
      req->__body[body_length] = '\0'; // Add null terminator
    } else {
      req->__body = (char *) malloc(1);
      req->__body[0] = '\0';
    }
  } else {
    req->__body = (char *) malloc(1);
    req->__body[0] = '\0';
  }
}

void display(const struct Request * req) {
  printf(
    "Request = {\n >\t"
    "__request = %s\n >\t"
    "__max_size = %d\n >\t"
    "__endpoint = %s\n >\t"
    "__method= %s\n >\t"
    "__param = %s\n >\t"
    "__body = %s\n >\t"
    "__header_count = %d\n >\t"
    "__base_endpoint_exists = %d\n >\t"
    "__base_endpoint = %s\n"
    "};\n",
    req->__request,
    req->__max_size,
    req->__endpoint,
    req->__method,
    req->__param,
    req->__body,
    req->__header_count,
    req->__base_endpoint_exists,
    req->__base_endpoint
  );

  printf("Request.__headers = \n");

  for (int row = 0; row < req->__header_count; row++) {
    printf("> %s\n", req->__headers[row]);
  }
}

int main () {
	setbuf(stdout, NULL);
  setbuf(stderr, NULL);

  int socket_fd = socket(AF_INET, SOCK_STREAM, 0);

  if (socket_fd == -1) {
    printf("Cannot Create Socket!\n");
    return 1;
  }

  int reuse = 1;

	if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
		printf("SO_REUSEADDR failed!\n");
		return 1;
	}

  struct sockaddr_in address = {
    .sin_family = AF_INET,
    .sin_port = htons(PORT) ,
    .sin_addr.s_addr = htonl(INADDR_ANY),
  };

  struct sockaddr_in client_address;
  int client_address_length = sizeof(client_address);

  int bind_success = bind(socket_fd, (struct sockaddr *) &address, sizeof(address));

  if (bind_success != 0) {
    printf("Cannot Bind Socket!\n");
    return 1;
  }

  int backlog = 5;
  int listening = listen(socket_fd, backlog);

  if (listening != 0) {
    printf("Cannot Listen!\n");
    return 1;
  }

  while (1) {
    int client_socket_fd = accept(socket_fd, (struct sockaddr *) &client_address, (socklen_t *) &client_address_length);
      
    if (client_socket_fd < 0) {
      perror("Accept failed");
      continue;
    }

    pid_t pid = fork();
    printf("Assigned Pid = %d\n", pid);

    if (pid < 0) {
      perror("Fork failed\n");
      close(client_socket_fd);
      continue;
    }

    if (pid == 0) {
      /* Child process - handle the client request */
      close(socket_fd); // Close listening socket in child

      char request[4096];
      char response[1024];


      int bytes_read = recv(client_socket_fd, (char *) request, sizeof (request), 0);

      if (bytes_read < 0) {
        printf("Cannot Read Incoming Request!\n");
        close(client_socket_fd);
        exit(1);
      }

      // Null-terminate the request
      request[bytes_read] = '\0';

      // FOR Stage 1, 2, 3, 4, 5
      struct Request req_for_home, req_for_user_agent, req_for_echo;
      
      parse_request(request, &req_for_home, "/");
      parse_request(request, &req_for_user_agent, "/user-agent");
      parse_request(request, &req_for_echo, "/echo");
      
      display(&req_for_home);
      printf("%s and %d\n", req_for_home.__endpoint, strncmp(req_for_home.__endpoint, "/", strlen(req_for_home.__endpoint)) == 0);
      
      printf("REQUEST = %s\n", request);
      
      if (req_for_echo.__base_endpoint_exists == 1) {
        printf("WELCOME TO ECHO\n");
        snprintf(
          response,
          sizeof(response),
          "HTTP/1.1 200 OK\r\n"
          "Content-Type: text/plain\r\n"
          "Content-Length: %lu\r\n\r\n"
          "%s",
          strlen(req_for_echo.__param),
          req_for_echo.__param
        );
      } 
      else if (req_for_user_agent.__base_endpoint_exists == 1) {
        printf("WELCOME TO USER-AGENT\n");
        char* header_user_agent;
      
        for (int idx = 0; idx < req_for_user_agent.__header_count; idx++) {
          char * header_present = strstr(req_for_user_agent.__headers[idx], "User-Agent: ");
          if (header_present != NULL) {
            char * header_value_start = header_present + strlen("User-Agent: ");
            char * header_value = (char *) malloc(strlen(req_for_user_agent.__headers[idx]) - strlen("User-Agent: ") + 1);
        
            strncpy(header_value, header_value_start, strlen(req_for_user_agent.__headers[idx]) - strlen("User-Agent: "));
            header_value[strlen(req_for_user_agent.__headers[idx]) - strlen("User-Agent: ")] = '\0';
      
            snprintf(
              response,
              sizeof(response),
              "HTTP/1.1 200 OK\r\n"
              "Content-Type: text/plain\r\n"
              "Content-Length: %lu\r\n\r\n"
              "%s",
              strlen(header_value),
              header_value
            );
            break;
          } 
          else {
            snprintf(
              response,
              sizeof(response),
              "HTTP/1.1 200 OK\r\n"
              "Content-Type: text/plain\r\n"
              "Content-Length: 0\r\n\r\n"
              ""
            );
          }
        }
      } else if (req_for_home.__base_endpoint_exists == 1 && strncmp(req_for_home.__endpoint, "/", strlen(req_for_home.__endpoint)) == 0) {
        printf("WELCOME TO HOME\n");
        snprintf(
          response,
          sizeof(response),
          "HTTP/1.1 200 OK\r\n"
          "Content-Type: text/plain\r\n"
          "Content-Length: 0\r\n\r\n"
        );
      }
      else {
        printf("WELCOME TO NOT-FOUND\n");
        snprintf(
          response,
          sizeof(response),
          "HTTP/1.1 404 Not Found\r\n\r\n"
          "Content-Type: text/plain\r\n"
          "Content-Length: 0\r\n\r\n"
        );
      
      }

      // For Stage 6
      snprintf(response, sizeof(response), "HTTP/1.1 200 OK\r\n\r\n");
      send(client_socket_fd, response, sizeof (response), 0);

      close(client_socket_fd);
      exit(0); // Child process exits after handling the request

    } else {
      // Parent process - continue accepting new connections
      close(client_socket_fd); // Close client socket in parent
    }
  }

  close(socket_fd);

  return 0;
}
