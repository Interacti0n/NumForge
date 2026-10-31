#ifndef NUMFORGE_WEB_WORKERS_H
#define NUMFORGE_WEB_WORKERS_H

#include "apps/web/server/web_socket.h"
#include "apps/web/http/http_request.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NUMFORGE_WEB_DEFAULT_WORKERS 4U
#define NUMFORGE_WEB_MAX_WORKERS 16U
#define NUMFORGE_WEB_DEFAULT_QUEUE 32U
#define NUMFORGE_WEB_MAX_QUEUE 64U
#define NUMFORGE_WEB_MAX_PENDING (NUMFORGE_WEB_MAX_QUEUE + NUMFORGE_WEB_MAX_WORKERS)
#define NUMFORGE_WEB_QUEUE_TIMEOUT_MS 1000U
#define NUMFORGE_WEB_REQUEST_TIMEOUT_MS 2000U

typedef struct NumForgeWebWorkers NumForgeWebWorkers;

/* Fixed general compute threads, one bounded-literal worker and one
 * nonblocking receiver. General workers retain FIFO service; the extra worker
 * may bypass general jobs only for requests classified by the server as
 * bounded literal arithmetic. Session leases/deadlines still apply. The original bound of
 * workers + capacity covers incomplete frames, ready jobs and active compute
 * together. Input buffers are allocated once (8 KiB plus metadata per slot).
 * A slot releases when a compute worker takes its owned frame snapshot.
 * Origin remains borrowed until
 * stop returns. Live-allocation tracking must stay disabled in request threads:
 * session values may change owners; current optional ledgers are thread-local.
 * NULL means invalid limits or allocation/synchronization/thread-start failure;
 * partial startup is joined and freed before returning. */
NumForgeWebWorkers *numforge_web_workers_start(unsigned int workers,
    unsigned int capacity, uint16_t port, const char *origin);
/* Private dispatch seam for deterministic scheduling/ownership checks.
 * Handler must be thread-safe and return before shutdown can finish. */
typedef void (*NumForgeWebRequestHandler)(NumForgeSocket, char *, const NumForgeHttpFrame *);
NumForgeWebWorkers *numforge_web_workers_start_handler(unsigned int workers,
    unsigned int capacity, uint16_t port, const char *origin, NumForgeWebRequestHandler handler);
/* Nonblocking admission of a configured nonblocking socket. May consume one
 * buffered chunk. Success transfers ownership; on failure the caller must
 * reject/close it. accepted_ms is monotonic. */
bool numforge_web_workers_submit(NumForgeWebWorkers *workers, NumForgeSocket socket, uint64_t accepted_ms);
/* Acceptor calls at least every 100 ms, including while all workers are busy.
 * Ready queue expires after 1 s or the original 2-s receive/admission deadline,
 * whichever comes first. now may precede a concurrent queue publication; such
 * newer entries remain eligible. Receiver expires incomplete frames independently. */
void numforge_web_workers_expire(NumForgeWebWorkers *workers, uint64_t now);
/* Incomplete + ready snapshots; active counts both kinds of compute owner.
 * Diagnostics/tests, not an admission guarantee. */
size_t numforge_web_workers_pending(NumForgeWebWorkers *workers, size_t *active, size_t *receiving);
/* Stop admission, join the receiver, reject/close incomplete/queued sockets,
 * join active owners, then free.
 * The caller must exclude other pool API calls during destruction. NULL is safe.
 * Active requests retain their socket/computation deadlines; never cancel them. */
void numforge_web_workers_stop(NumForgeWebWorkers *workers);

#endif
