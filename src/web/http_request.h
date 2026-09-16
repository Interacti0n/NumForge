#ifndef NUMFORGE_HTTP_REQUEST_H
#define NUMFORGE_HTTP_REQUEST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
------------------------------------------------------------------------------------------------------------------------------
    Bounded HTTP framing. MORE means append bytes and try again.
    Metadata is usable only after READY. Body bytes are validated by the caller.
    One request is consumed; trailing bytes are ignored (no keep-alive).

    Implementation: src/web/http_request.c
------------------------------------------------------------------------------------------------------------------------------
*/
#define NUMFORGE_WEB_REQUEST_CAPACITY 8192U

typedef enum NumForgeHttpStatus
{
    NUMFORGE_HTTP_MORE,
    NUMFORGE_HTTP_READY,
    NUMFORGE_HTTP_BAD,
    NUMFORGE_HTTP_LARGE
} NumForgeHttpStatus;

typedef struct NumForgeHttpFrame
{
    size_t length;
    size_t header_length;
    bool has_content_length;
    bool origin_allowed;
    char method[16];
    char target[128];
} NumForgeHttpFrame;

NumForgeHttpStatus numforge_http_probe(
    const char *data,
    size_t used,
    size_t capacity,
    uint16_t port,
    NumForgeHttpFrame *frame
);
bool numforge_request_target(
    const char *request,
    char *method,
    size_t method_capacity,
    char *target,
    size_t target_capacity
);

#endif
