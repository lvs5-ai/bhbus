#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include <stddef.h>

typedef struct {
    char *data;
    size_t size;
} HttpBuffer;

char *fetch_url(const char *url, long timeout_seconds);

#endif