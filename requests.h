#ifndef _REQUESTS_
#define _REQUESTS_

#include "parson.h"

// Computes and returns a DELETE request.
char *compute_delete_request(char *host, char *url, const char *token);

// Computes and returns a GET request string (query_params and cookies can be set to NULL if not needed)
char *compute_get_request(char *host, char *url,
 char *query_params, char **cookies, int cookies_count,
  const char *token);

// Computes and returns a POST request string (cookies can be NULL if not needed)
char *compute_post_request(char *host, char *url, 
char *content_type, JSON_Value *json_value, char **cookies, 
int cookies_count, const char *token);

#endif
