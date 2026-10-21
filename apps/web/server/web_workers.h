#ifndef NUMFORGE_WEB_WORKERS_H
#define NUMFORGE_WEB_WORKERS_H

#include "apps/web/server/web_socket.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NUMFORGE_WEB_DEFAULT_WORKERS 4U
#define NUMFORGE_WEB_MAX_WORKERS 16U
#define NUMFORGE_WEB_DEFAULT_QUEUE 32U
#define NUMFORGE_WEB_MAX_QUEUE 64U
#define NUMFORGE_WEB_QUEUE_TIMEOUT_MS 1000U
#define NUMFORGE_WEB_REQUEST_TIMEOUT_MS 2000U

typedef struct NumForgeWebWorkers NumForgeWebWorkers;

/* Fixed threads and socket slots, allocated once. Origin remains borrowed until
 * stop returns. Live-allocation tracking must stay disabled in request threads:
 * session values may change owners; current optional ledgers are thread-local.
 * NULL means invalid limits or allocation/synchronization/thread-start failure;
 * partial startup is joined and freed before returning. */
NumForgeWebWorkers *numforge_web_workers_start(unsigned int workers,
    unsigned int capacity, uint16_t port, const char *origin);
/* Nonblocking admission of a configured nonblocking socket. Success transfers
 * ownership; on failure the caller must reject/close it. accepted_ms is monotonic. */
bool numforge_web_workers_submit(NumForgeWebWorkers *workers, NumForgeSocket socket, uint64_t accepted_ms);
/* Acceptor calls at least every 100 ms, including while all workers are busy.
 * Timestamps supplied by the single acceptor must be in acceptance order. */
void numforge_web_workers_expire(NumForgeWebWorkers *workers, uint64_t now);
/* Snapshot for diagnostics/tests, not an admission guarantee. */
size_t numforge_web_workers_pending(NumForgeWebWorkers *workers, size_t *active);
/* Stop admission, reject/close queued sockets, join active owners, then free.
 * The caller must exclude other pool API calls during destruction. NULL is safe.
 * Active requests retain their socket/computation deadlines; never cancel them. */
void numforge_web_workers_stop(NumForgeWebWorkers *workers);

#endif
