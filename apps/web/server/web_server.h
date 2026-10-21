#ifndef NUMFORGE_WEB_SERVER_H
#define NUMFORGE_WEB_SERVER_H

#include "apps/web/server/web_socket.h"

#include <stdint.h>
#include <stdbool.h>

/* Socket is nonblocking; receive deadline includes time spent in the queue. */
void numforge_handle_connection_before(NumForgeSocket socket, uint16_t port, const char *public_origin, uint64_t deadline);
/* Best-effort nonblocking 503; caller still owns and must close the socket. */
void numforge_reject_connection(NumForgeSocket socket);
/* Only after stopping/joining all connection handlers. */
bool numforge_web_server_shutdown(void);

#endif
