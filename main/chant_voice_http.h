#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    CHANT_HTTP_OK, CHANT_HTTP_READ_ERROR, CHANT_HTTP_TIMEOUT,
    CHANT_HTTP_TOO_LARGE, CHANT_HTTP_CANCELLED
} chant_http_result_t;
/* The adapter maps a temporary network read timeout to CHANT_HTTP_AGAIN. */
#define CHANT_HTTP_AGAIN (-2)
typedef struct {
    void *context;
    int (*read)(void *, char *, size_t);
    bool (*complete)(void *);
    uint64_t (*milliseconds)(void *);
    bool (*cancelled)(void *);
    void (*pause)(void *);
} chant_http_reader_t;
chant_http_result_t chant_http_read_body(const chant_http_reader_t *io,
                                       char *out, size_t capacity,
                                       uint64_t timeout_ms);
/* At most one retry; uploading requires a stable idempotency key. */
bool chant_http_retry(unsigned attempt, int status, bool transport_failure,
                      bool upload, bool has_request_id);
