#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "../../src/web/http_request.h"

/* Arbitrary bytes and selected partial prefixes must never expose metadata
 * outside the supplied buffer. No sockets or calculation are involved. */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size >= NUMFORGE_WEB_REQUEST_CAPACITY) return 0;
    NumForgeHttpFrame a, b;
    NumForgeHttpStatus status = numforge_http_probe((const char *)data, size,
        NUMFORGE_WEB_REQUEST_CAPACITY, 8765, &a);
    if (status != numforge_http_probe((const char *)data, size,
        NUMFORGE_WEB_REQUEST_CAPACITY, 8765, &b)) abort();
    if (status == NUMFORGE_HTTP_READY)
    {
        if (a.header_length > a.length || a.length > size || a.length - a.header_length > 4096U ||
            a.length != b.length || strcmp(a.target, b.target) != 0) abort();
        if (a.length > 0 && numforge_http_probe((const char *)data, a.length - 1U,
            NUMFORGE_WEB_REQUEST_CAPACITY, 8765, &b) != NUMFORGE_HTTP_MORE) abort();
    }
    (void)numforge_http_probe((const char *)data, size / 2U,
        NUMFORGE_WEB_REQUEST_CAPACITY, 80, &b);
    return 0;
}
