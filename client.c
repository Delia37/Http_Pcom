#include <stdio.h>      /* printf, sprintf */
#include <stdlib.h>     /* exit, atoi, malloc, free */
#include <unistd.h>     /* read, write, close */
#include <string.h>     /* memcpy, memset */
#include <sys/socket.h> /* socket, connect */
#include <netinet/in.h> /* struct sockaddr_in, struct sockaddr */
#include <netdb.h>      /* struct hostent, gethostbyname */
#include <arpa/inet.h>
#include "helpers.h"
#include "requests.h"
#include "parson.h"
#include <ctype.h>

// IP, port
char server_ip[] = "34.246.184.49";
int server_port = 8080;

// Rutele de acces
#define CONTENT_TYPE "application/json"
#define REGISTER_URL "/api/v1/tema/auth/register"
#define LOGIN_URL "/api/v1/tema/auth/login"
#define ACCESS_URL "/api/v1/tema/library/access"
#define BOOKS_URL "/api/v1/tema/library/books"
#define LOGOUT_URL "/api/v1/tema/auth/logout"

// Functie pentru crearea payload-ului
JSON_Value *create_json_payload(const char *username,
                                const char *password)
{
  JSON_Value *root_value = json_value_init_object();
  JSON_Object *root_object = json_value_get_object(root_value);
  json_object_set_string(root_object, "username", username);
  json_object_set_string(root_object, "password", password);
  return root_value;
}

int main(int argc, char *argv[])
{
  char *message;
  char *response;
  int sockfd;
  char command[50];

  char session_cookie[1024] = "";
  char jwt_token[1024] = "";

  // loop infinit, se iese la exit
  while (1)
  {
    printf("Enter command: ");
    scanf("%s", command);

    if (strcmp(command, "register") == 0)
    {
      char username[50], password[50];

      void clear_input_buffer()
      {
        int c;
        while ((c = getchar()) != '\n' && c != EOF)
        {
        }
      }

      // iau enter-ul
      getchar();

      printf("username= ");

      if (fgets(username, sizeof(username), stdin) == NULL)
      {
        clear_input_buffer();
      }

      // verificare spatii la username
      if (strchr(username, ' ') != NULL)
      {
        printf("error: Username must not contain spaces.\n");
      }
      username[strcspn(username, "\n")] = '\0'; // stergere newline

      printf("password= ");

      if (fgets(password, sizeof(password), stdin) == NULL)
      {
        clear_input_buffer();
      }

      // verificare spatii la parola
      if (strchr(password, ' ') != NULL)
      {
        printf("error: Password must not contain spaces.\n");
        exit(EXIT_FAILURE); // exit daca parola e invalida
      }
      password[strcspn(password, "\n")] = '\0';

      // deschid conexiunea catre server
      sockfd = open_connection(server_ip, server_port, AF_INET, SOCK_STREAM, 0);

      // creez payload-ul
      JSON_Value *root_value = create_json_payload(username, password);

      // generare request si trimitere cerere
      message = compute_post_request(server_ip, REGISTER_URL, CONTENT_TYPE, root_value, NULL, 0, "");
      send_to_server(sockfd, message);

      // iau raspunsul de la server
      response = receive_from_server(sockfd);

      close(sockfd);
      printf("Response: %s\n", response);

      // eliberez memoria
      json_value_free(root_value);
      free(message);
      free(response);
    }
    else if (strcmp(command, "login") == 0)
    {
      char username[50], password[50];
      printf("username=");
      scanf("%s", username);
      printf("password=");
      scanf("%s", password);

      sockfd = open_connection(server_ip, server_port, AF_INET, SOCK_STREAM, 0);

      JSON_Value *root_value = create_json_payload(username, password);

      message = compute_post_request(server_ip, LOGIN_URL, CONTENT_TYPE, root_value, NULL, 0, "");
      send_to_server(sockfd, message);

      response = receive_from_server(sockfd);

      // extrag cookie-ul din raspuns
      const char *cookie_header = "Set-Cookie: ";
      char *cookie_start = strstr(response, cookie_header);
      if (cookie_start)
      {
        cookie_start += strlen(cookie_header);
        char *cookie_end = strchr(cookie_start, ';');
        if (cookie_end)
        {
          size_t cookie_length = cookie_end - cookie_start;
          if (cookie_length < sizeof(session_cookie))
          {
            strncpy(session_cookie, cookie_start, cookie_length);
            session_cookie[cookie_length] = '\0';
          }
        }
      }

      close(sockfd);
      json_value_free(root_value);
      printf("Response: %s\n", response);
      free(message);
      free(response);
    }
    else if (strcmp(command, "enter_library") == 0)
    {
      sockfd = open_connection(server_ip, server_port, AF_INET, SOCK_STREAM, 0);

      char url[] = ACCESS_URL;
      char *cookies[] = {
          session_cookie};

      message = compute_get_request(server_ip, url, NULL, cookies, sizeof(cookies) / sizeof(char **), "");
      send_to_server(sockfd, message);

      response = receive_from_server(sockfd);
      close(sockfd);

      // extargere token JWL token din raspuns
      const char *token_prefix = "\"token\":\"";
      char *start = strstr(response, token_prefix);
      if (start)
      {
        start += strlen(token_prefix);
        char *end = strchr(start, '\"');
        if (end)
        {
          size_t token_length = end - start;
          if (token_length < sizeof(jwt_token))
          {
            strncpy(jwt_token, start, token_length);
            jwt_token[token_length] = '\0';
          }
        }
      }

      printf("Response: %s\n", response);
      if (strlen(jwt_token) > 0)
      {
        printf("JWT Token: %s\n", jwt_token);
      }
      free(message);
      free(response);
    }
    else if (strcmp(command, "get_books") == 0)
    {
      sockfd = open_connection(server_ip, server_port, AF_INET, SOCK_STREAM, 0);
      char url[] = BOOKS_URL;

      message = compute_get_request(server_ip, url, NULL, NULL, 0, jwt_token);
      send_to_server(sockfd, message);

      response = receive_from_server(sockfd);
      close(sockfd);
      printf("Response: %s\n", response);
    }
    else if (strcmp(command, "add_book") == 0)
    {
      char title[256], author[256], genre[256], publisher[256], page_count[256];

      getchar();
      printf("title=");
      fgets(title, sizeof(title), stdin);
      for (int i = 0; title[i] != '\0'; i++)
      {
        if (title[i] == '\n')
        {
          title[i] = '\0';
          break;
        }
      }

      printf("author=");
      fgets(author, sizeof(author), stdin);
      for (int i = 0; author[i] != '\0'; i++)
      {
        if (author[i] == '\n')
        {
          author[i] = '\0';
          break;
        }
      }

      printf("genre=");
      fgets(genre, sizeof(genre), stdin);
      for (int i = 0; genre[i] != '\0'; i++)
      {
        if (genre[i] == '\n')
        {
          genre[i] = '\0';
          break;
        }
      }

      printf("publisher=");
      fgets(publisher, sizeof(publisher), stdin);
      for (int i = 0; publisher[i] != '\0'; i++)
      {
        if (publisher[i] == '\n')
        {
          publisher[i] = '\0';
          break;
        }
      }

      printf("page_count=");
      fgets(page_count, sizeof(page_count), stdin);
      for (int i = 0; page_count[i] != '\0'; i++)
      {
        if (page_count[i] == '\n')
        {
          page_count[i] = '\0';
          break;
        }
      }

      // verificare - page_count trebuie sa fie numar
      for (int i = 0; page_count[i] != '\0'; i++)
      {
        if (!isdigit(page_count[i]))
        {
          printf("error: Page count must be a numeric value.\n");
          break;
        }
      }

      sockfd = open_connection(server_ip, server_port, AF_INET, SOCK_STREAM, 0);

      // generare payload
      JSON_Value *root_value = json_value_init_object();
      JSON_Object *root_object = json_value_get_object(root_value);
      json_object_set_string(root_object, "title", title);
      json_object_set_string(root_object, "author", author);
      json_object_set_string(root_object, "genre", genre);
      json_object_set_number(root_object, "page_count", atoi(page_count));
      json_object_set_string(root_object, "publisher", publisher);

      char url[] = BOOKS_URL;
      char content_type[] = CONTENT_TYPE;

      message = compute_post_request(server_ip, url, content_type, root_value, NULL, 0, jwt_token);
      send_to_server(sockfd, message);

      response = receive_from_server(sockfd);
      close(sockfd);
      printf("Response: %s\n", response);
    }
    else if (strcmp(command, "get_book") == 0)
    {
      char id[10];
      printf("id=");
      scanf("%s", id);

      sockfd = open_connection(server_ip, server_port, AF_INET, SOCK_STREAM, 0);

      // buffer pentru URL
      char url[256] = "";
      if (sizeof(BOOKS_URL) + strlen(id) + 2 < sizeof(url))
      {
        strcpy(url, BOOKS_URL);
        strcat(url, "/");
        strcat(url, id);
      }
      else
      {
        fprintf(stderr, "URL buffer too small!\n");
        close(sockfd);
        continue;
      }

      message = compute_get_request(server_ip, url, NULL, NULL, 0, jwt_token);
      send_to_server(sockfd, message);

      response = receive_from_server(sockfd);
      close(sockfd);
      printf("Response: %s\n", response);
    }
    else if (strcmp(command, "delete_book") == 0)
    {
      char id[10];
      printf("id=");
      scanf("%s", id);

      sockfd = open_connection(server_ip, server_port, AF_INET, SOCK_STREAM, 0);

      char url[256];
      if (sizeof(BOOKS_URL) + strlen(id) + 2 < sizeof(url))
      {
        // construiesc buffer-ul
        strcpy(url, BOOKS_URL);
        strcat(url, "/");
        strcat(url, id);
      }
      else
      {
        fprintf(stderr, "URL buffer too small!\n");
        close(sockfd);
        continue;
      }

      message = compute_delete_request(server_ip, url, jwt_token);
      send_to_server(sockfd, message);

      response = receive_from_server(sockfd);
      close(sockfd);
      printf("Response: %s\n", response);
    }
    else if (strcmp(command, "logout") == 0)
    {
      char url[] = LOGOUT_URL;
      char *cookies[] = {
          session_cookie};
      message = compute_get_request(server_ip, url, NULL, cookies, sizeof(cookies) / sizeof(char **), "");

      sockfd = open_connection(server_ip, server_port, AF_INET, SOCK_STREAM, 0);

      send_to_server(sockfd, message);

      response = receive_from_server(sockfd);
      close(sockfd);
      printf("Response: %s\n", response);
      strcpy(session_cookie, "");
      strcpy(jwt_token, "");
    }
    else if (strcmp(command, "exit") == 0)
      break;
  }
  return 0;
}