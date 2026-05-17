#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

char *fetch_url(const char *url, long timeout_seconds);
int post_json(const char *url, const char *json_payload, long timeout_seconds);

#endif
