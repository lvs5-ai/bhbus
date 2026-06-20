#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <curl/curl.h>
#include "http_client.h"

typedef struct {
    char *data;
    size_t size;
} HttpBuffer;

static size_t write_cb(void *ptr, size_t size, size_t nmemb, void *userp) {
    size_t real_size = size * nmemb;
    HttpBuffer *buffer = (HttpBuffer *)userp;

    char *new_data = realloc(buffer->data, buffer->size + real_size + 1);
    if (!new_data) {
        return 0;
    }

    buffer->data = new_data;
    memcpy(buffer->data + buffer->size, ptr, real_size);
    buffer->size += real_size;
    buffer->data[buffer->size] = '\0';
    return real_size;
}

char *fetch_url(const char *url, long timeout_seconds) {
    CURL *curl = curl_easy_init();
    if (!curl) {
        return NULL;
    }

    HttpBuffer buffer = {0};
    buffer.data = malloc(1);
    if (!buffer.data) {
        curl_easy_cleanup(curl);
        return NULL;
    }
    buffer.data[0] = '\0';

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_seconds);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "BH-BUS/1.0");

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        free(buffer.data);
        return NULL;
    }

    return buffer.data;
}

int post_json(const char *url, const char *json_payload, long timeout_seconds) {
    CURL *curl = curl_easy_init();
    if (!curl) {
        return 0;
    }

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_payload);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_seconds);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "BH-BUS/1.0");

    CURLcode res = curl_easy_perform(curl);
    long status_code = 0;
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);
    } else {
        fprintf(stderr, "Falha HTTP ao enviar snapshot: %s\n", curl_easy_strerror(res));
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res == CURLE_OK && (status_code < 200 || status_code >= 300)) {
        fprintf(stderr, "Servidor rejeitou o snapshot com HTTP %ld.\n", status_code);
        return 0;
    }

    return res == CURLE_OK;
}
