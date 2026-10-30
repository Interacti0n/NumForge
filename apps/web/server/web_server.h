#ifndef NUMFORGE_WEB_SERVER_H
#define NUMFORGE_WEB_SERVER_H

#include "apps/web/server/web_socket.h"
#include "apps/web/http/http_request.h"

#include <stdint.h>
#include <stdbool.h>

/* Socket is nonblocking; receive deadline includes time spent in the queue. */
void numforge_handle_connection_before(NumForgeSocket socket, uint16_t port, const char *public_origin, uint64_t deadline);
/* Private handoff: complete validated frame, mutable NUL-terminated request.
 * No receive calls. The worker owns the buffer/socket for this entire call. */
void numforge_handle_framed_connection(NumForgeSocket socket, char *request, const NumForgeHttpFrame *frame);
/* Best-effort nonblocking framing error (400/408/413), then caller closes. */
void numforge_reject_request(NumForgeSocket socket, int status);
/* Best-effort nonblocking 503; caller still owns and must close the socket. */
void numforge_reject_connection(NumForgeSocket socket);
/* Periodic monotonic sweep; also performed during client acquisition. */
void numforge_web_server_expire(uint64_t now);
/* Only after stopping/joining all connection handlers. */
bool numforge_web_server_shutdown(void);

#endif
