#include "apps/web/server/web_workers.h"
#include "apps/web/server/web_server.h"
#include <numforge/runtime.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <process.h>
#else
#include <errno.h>
#include <pthread.h>
#include <sys/select.h>
#include <unistd.h>
#endif

typedef enum ConnectionState { CONNECTION_EMPTY, CONNECTION_READING, CONNECTION_READY } ConnectionState;
typedef struct WebConnection {
    NumForgeSocket socket;
    uint64_t accepted_ms, queued_ms;
    size_t used;
    ConnectionState state;
    NumForgeHttpFrame frame;
    char request[NUMFORGE_WEB_REQUEST_CAPACITY];
} WebConnection;

struct NumForgeWebWorkers {
#ifdef _WIN32
    SRWLOCK lock;
    CONDITION_VARIABLE changed, incoming;
    HANDLE threads[NUMFORGE_WEB_MAX_WORKERS], receiver;
#else
    pthread_mutex_t lock;
    pthread_cond_t changed, incoming;
    pthread_t threads[NUMFORGE_WEB_MAX_WORKERS], receiver;
#endif
    size_t queue[NUMFORGE_WEB_MAX_PENDING];
    size_t head, count, reading, active, started, capacity;
    bool stopping, receiver_started;
    uint16_t port;
    const char *origin;
    NumForgeWebRequestHandler handler;
    WebConnection connections[];
};

static void lock_workers(NumForgeWebWorkers *workers)
{
#ifdef _WIN32
    AcquireSRWLockExclusive(&workers->lock);
#else
    if (pthread_mutex_lock(&workers->lock) != 0) abort();
#endif
}
static void unlock_workers(NumForgeWebWorkers *workers)
{
#ifdef _WIN32
    ReleaseSRWLockExclusive(&workers->lock);
#else
    if (pthread_mutex_unlock(&workers->lock) != 0) abort();
#endif
}
static void notify_worker(NumForgeWebWorkers *workers)
{
#ifdef _WIN32
    WakeConditionVariable(&workers->changed);
#else
    if (pthread_cond_signal(&workers->changed) != 0) abort();
#endif
}
static void notify_receiver(NumForgeWebWorkers *workers)
{
#ifdef _WIN32
    WakeConditionVariable(&workers->incoming);
#else
    if (pthread_cond_signal(&workers->incoming) != 0) abort();
#endif
}
static bool deadline_elapsed(uint64_t accepted, uint64_t now, uint64_t limit)
{
    return now < accepted || now - accepted >= limit;
}
static bool connection_expired(const WebConnection *connection, uint64_t now)
{
    return deadline_elapsed(connection->queued_ms, now, NUMFORGE_WEB_QUEUE_TIMEOUT_MS) ||
        deadline_elapsed(connection->accepted_ms, now, NUMFORGE_WEB_REQUEST_TIMEOUT_MS);
}
static bool receive_retryable(void)
{
#ifdef _WIN32
    int error = WSAGetLastError();
    return error == WSAEWOULDBLOCK || error == WSAEINTR;
#else
    return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR;
#endif
}

/* One nonblocking read/probe: 0 needs more bytes, 200 is a complete frame,
 * otherwise a framing error. The caller exclusively owns this descriptor. */
static int receive_frame(WebConnection *connection, const NumForgeWebWorkers *workers)
{
    size_t room = sizeof(connection->request) - connection->used - 1U;
    if (!room) return 413;
    int received = recv(connection->socket, connection->request + connection->used, (int)room, 0);
    if (received < 0 && receive_retryable()) return 0;
    if (received <= 0) return 400;
    connection->used += (size_t)received;
    NumForgeHttpStatus status = numforge_http_probe_with_origin(connection->request,
        connection->used, sizeof(connection->request), workers->port, workers->origin, &connection->frame);
    if (status == NUMFORGE_HTTP_MORE) return 0;
    if (status != NUMFORGE_HTTP_READY) return status == NUMFORGE_HTTP_LARGE ? 413 : 400;
    connection->request[connection->frame.length] = '\0';
    return 200;
}

/* Only this thread reads slots published as READING. Submission may consume
 * one initial buffered chunk before publishing. A slot stays READING until
 * its complete frame is published under the lock, or its socket is detached.
 * Workers copy READY slots before making them EMPTY. Stop joins this receiver
 * before touching its slots. No socket operation holds the pool lock. */
#ifdef _WIN32
static unsigned int __stdcall receive_connections(void *argument)
#else
static void *receive_connections(void *argument)
#endif
{
    NumForgeWebWorkers *workers = argument;
    for (;;) {
        size_t slots[NUMFORGE_WEB_MAX_PENDING], count = 0U;
        int readable[NUMFORGE_WEB_MAX_PENDING] = {0};
        lock_workers(workers);
        while (!workers->reading && !workers->stopping) {
#ifdef _WIN32
            if (!SleepConditionVariableSRW(&workers->incoming, &workers->lock, INFINITE, 0)) abort();
#else
            if (pthread_cond_wait(&workers->incoming, &workers->lock) != 0) abort();
#endif
        }
        if (workers->stopping) { unlock_workers(workers); break; }
        for (size_t i = 0U; i < workers->capacity; ++i) {
            if (workers->connections[i].state != CONNECTION_READING) continue;
            slots[count++] = i;
        }
        unlock_workers(workers);
        /* A new submission wakes an idle receiver. During partial requests,
         * bound time to notice additional sockets without an extra wake socket. */
        /* Winsock's default fd_set holds 64 sockets, while admission can hold
         * 16 workers + 64 queued inputs. Bound each set explicitly; only the
         * first batch waits, so extra batches do not extend the poll interval. */
        const size_t batch_limit = FD_SETSIZE < NUMFORGE_WEB_MAX_PENDING ? FD_SETSIZE : NUMFORGE_WEB_MAX_PENDING;
        for (size_t offset = 0U; offset < count; offset += batch_limit) {
            size_t end = count - offset < batch_limit ? count : offset + batch_limit;
            fd_set ready;
            NumForgeSocket largest = 0;
            FD_ZERO(&ready);
            for (size_t i = offset; i < end; ++i) {
                NumForgeSocket socket = workers->connections[slots[i]].socket;
                FD_SET(socket, &ready);
                if (socket > largest) largest = socket;
            }
            struct timeval timeout = {0, offset ? 0L : 10000L};
#ifdef _WIN32
            (void)largest;
            int selected = select(0, &ready, NULL, NULL, &timeout);
#else
            int selected = select(largest + 1, &ready, NULL, NULL, &timeout);
#endif
            if (selected < 0 && receive_retryable()) continue;
            for (size_t i = offset; i < end; ++i)
                readable[i] = selected < 0 ? -1 : selected && FD_ISSET(workers->connections[slots[i]].socket, &ready) ? 1 : 0;
        }
        for (size_t i = 0U; i < count; ++i) {
            size_t slot = slots[i];
            WebConnection *connection = &workers->connections[slot];
            NumForgeSocket socket = connection->socket;
            int error = 0;
            uint64_t now = numforge_monotonic_ms();
            if (deadline_elapsed(connection->accepted_ms, now, NUMFORGE_WEB_REQUEST_TIMEOUT_MS)) error = 408;
            else if (readable[i] < 0) error = 400;
            else if (readable[i]) {
                int status = receive_frame(connection, workers);
                if (status >= 400) error = status;
                else if (status == 200) {
                    lock_workers(workers);
                    connection->queued_ms = numforge_monotonic_ms();
                    connection->state = CONNECTION_READY;
                    --workers->reading;
                    workers->queue[(workers->head + workers->count) % workers->capacity] = slot;
                    ++workers->count;
                    notify_worker(workers);
                    unlock_workers(workers);
                }
            }
            if (error) {
                lock_workers(workers);
                connection->state = CONNECTION_EMPTY;
                --workers->reading;
                unlock_workers(workers);
                numforge_reject_request(socket, error);
                numforge_close_socket(socket);
            }
        }
    }
    return 0;
}

#ifdef _WIN32
static unsigned int __stdcall serve_connections(void *argument)
#else
static void *serve_connections(void *argument)
#endif
{
    NumForgeWebWorkers *workers = argument;
    for (;;) {
        lock_workers(workers);
        while (!workers->count && !workers->stopping) {
#ifdef _WIN32
            if (!SleepConditionVariableSRW(&workers->changed, &workers->lock, INFINITE, 0)) abort();
#else
            if (pthread_cond_wait(&workers->changed, &workers->lock) != 0) abort();
#endif
        }
        if (workers->stopping) { unlock_workers(workers); break; }
        size_t slot = workers->queue[workers->head];
        WebConnection connection = workers->connections[slot];
        workers->connections[slot].state = CONNECTION_EMPTY;
        workers->head = (workers->head + 1U) % workers->capacity;
        --workers->count; ++workers->active;
        unlock_workers(workers);

        if (connection_expired(&connection, numforge_monotonic_ms()))
            numforge_reject_connection(connection.socket);
        else workers->handler(connection.socket, connection.request, &connection.frame);
        numforge_close_socket(connection.socket);

        lock_workers(workers);
        --workers->active;
        unlock_workers(workers);
    }
    return 0;
}

NumForgeWebWorkers *numforge_web_workers_start(unsigned int count,
    unsigned int capacity, uint16_t port, const char *origin)
{
    return numforge_web_workers_start_handler(count, capacity, port, origin, numforge_handle_framed_connection);
}

NumForgeWebWorkers *numforge_web_workers_start_handler(unsigned int count,
    unsigned int capacity, uint16_t port, const char *origin, NumForgeWebRequestHandler handler)
{
    if (!count || count > NUMFORGE_WEB_MAX_WORKERS || !capacity || capacity > NUMFORGE_WEB_MAX_QUEUE || !handler) return NULL;
    size_t slots = (size_t)capacity + count;
    NumForgeWebWorkers *workers = calloc(1U, sizeof(*workers) + slots * sizeof(WebConnection));
    if (!workers) return NULL;
    workers->capacity = slots; workers->port = port; workers->origin = origin;
    workers->handler = handler;
#ifdef _WIN32
    InitializeSRWLock(&workers->lock);
    InitializeConditionVariable(&workers->changed);
    InitializeConditionVariable(&workers->incoming);
#else
    if (pthread_mutex_init(&workers->lock, NULL) != 0) { free(workers); return NULL; }
    if (pthread_cond_init(&workers->changed, NULL) != 0) {
        (void)pthread_mutex_destroy(&workers->lock); free(workers); return NULL;
    }
    if (pthread_cond_init(&workers->incoming, NULL) != 0) {
        (void)pthread_cond_destroy(&workers->changed);
        (void)pthread_mutex_destroy(&workers->lock); free(workers); return NULL;
    }
#endif
    for (; workers->started < count; ++workers->started) {
#ifdef _WIN32
        workers->threads[workers->started] = (HANDLE)_beginthreadex(NULL, 0, serve_connections, workers, 0, NULL);
        if (!workers->threads[workers->started]) break;
#else
        if (pthread_create(&workers->threads[workers->started], NULL, serve_connections, workers) != 0) break;
#endif
    }
    if (workers->started == count) {
#ifdef _WIN32
        workers->receiver = (HANDLE)_beginthreadex(NULL, 0, receive_connections, workers, 0, NULL);
        workers->receiver_started = workers->receiver != NULL;
#else
        workers->receiver_started = pthread_create(&workers->receiver, NULL, receive_connections, workers) == 0;
#endif
        if (workers->receiver_started) return workers;
    }
    numforge_web_workers_stop(workers);
    return NULL;
}

bool numforge_web_workers_submit(NumForgeWebWorkers *workers, NumForgeSocket socket, uint64_t accepted_ms)
{
    if (!workers || socket == NUMFORGE_INVALID_SOCKET || accepted_ms > UINT64_MAX - NUMFORGE_WEB_REQUEST_TIMEOUT_MS) return false;
#ifndef _WIN32
    if (socket < 0 || socket >= FD_SETSIZE) return false;
#endif
    lock_workers(workers);
    bool accepted = !workers->stopping && workers->count + workers->reading + workers->active < workers->capacity;
    unlock_workers(workers);
    if (!accepted) return false;
    /* Already-buffered requests bypass the receiver's poll interval. Reading
     * once never waits for a peer and keeps all framing outside the pool lock.
     * A final admission check also supports simultaneous submitters. */
    WebConnection incoming = {0};
    incoming.socket = socket; incoming.accepted_ms = accepted_ms;
    int status = deadline_elapsed(accepted_ms, numforge_monotonic_ms(), NUMFORGE_WEB_REQUEST_TIMEOUT_MS) ?
        408 : receive_frame(&incoming, workers);
    lock_workers(workers);
    accepted = !workers->stopping && workers->count + workers->reading + workers->active < workers->capacity;
    if (accepted) {
        if (status < 400) {
            size_t slot = 0U;
            while (workers->connections[slot].state != CONNECTION_EMPTY) ++slot;
            workers->connections[slot] = incoming;
            WebConnection *connection = &workers->connections[slot];
            if (status == 200) {
                connection->state = CONNECTION_READY; connection->queued_ms = numforge_monotonic_ms();
                workers->queue[(workers->head + workers->count) % workers->capacity] = slot;
                ++workers->count; notify_worker(workers);
            } else {
                connection->state = CONNECTION_READING; ++workers->reading; notify_receiver(workers);
            }
        }
    }
    unlock_workers(workers);
    if (accepted && status >= 400) {
        numforge_reject_request(socket, status); numforge_close_socket(socket);
    }
    return accepted;
}

void numforge_web_workers_expire(NumForgeWebWorkers *workers, uint64_t now)
{
    if (!workers) return;
    NumForgeSocket expired[NUMFORGE_WEB_MAX_PENDING];
    size_t count = 0U;
    lock_workers(workers);
    /* Completion order may differ from acceptance order. The original receive
     * deadline can therefore expire a later job before the queue head. */
    size_t kept = 0U;
    for (size_t i = 0U; i < workers->count; ++i) {
        size_t slot = workers->queue[(workers->head + i) % workers->capacity];
        WebConnection *connection = &workers->connections[slot];
        if (connection_expired(connection, now)) {
            expired[count++] = connection->socket; connection->state = CONNECTION_EMPTY;
        } else workers->queue[(workers->head + kept++) % workers->capacity] = slot;
    }
    workers->count = kept;
    unlock_workers(workers);
    for (size_t i = 0U; i < count; ++i) {
        numforge_reject_connection(expired[i]); numforge_close_socket(expired[i]);
    }
}

size_t numforge_web_workers_pending(NumForgeWebWorkers *workers, size_t *active, size_t *receiving)
{
    if (!workers) { if (active) *active = 0U; if (receiving) *receiving = 0U; return 0U; }
    lock_workers(workers);
    size_t count = workers->count + workers->reading;
    if (active) *active = workers->active;
    if (receiving) *receiving = workers->reading;
    unlock_workers(workers);
    return count;
}

void numforge_web_workers_stop(NumForgeWebWorkers *workers)
{
    if (!workers) return;
    lock_workers(workers);
    workers->stopping = true;
#ifdef _WIN32
    WakeAllConditionVariable(&workers->changed); WakeAllConditionVariable(&workers->incoming);
#else
    if (pthread_cond_broadcast(&workers->changed) != 0 || pthread_cond_broadcast(&workers->incoming) != 0) abort();
#endif
    unlock_workers(workers);
    /* Join the sole reader before detaching its buffers/sockets. Active workers
     * own independent snapshots and retain their computation/send deadlines. */
    if (workers->receiver_started) {
#ifdef _WIN32
        if (WaitForSingleObject(workers->receiver, INFINITE) != WAIT_OBJECT_0 || !CloseHandle(workers->receiver)) abort();
#else
        if (pthread_join(workers->receiver, NULL) != 0) abort();
#endif
    }
    for (size_t i = 0U; i < workers->capacity; ++i) {
        if (workers->connections[i].state == CONNECTION_EMPTY) continue;
        numforge_reject_connection(workers->connections[i].socket);
        numforge_close_socket(workers->connections[i].socket);
    }
    for (size_t i = 0U; i < workers->started; ++i) {
#ifdef _WIN32
        if (WaitForSingleObject(workers->threads[i], INFINITE) != WAIT_OBJECT_0 || !CloseHandle(workers->threads[i])) abort();
#else
        if (pthread_join(workers->threads[i], NULL) != 0) abort();
#endif
    }
#ifndef _WIN32
    if (pthread_cond_destroy(&workers->incoming) != 0 || pthread_cond_destroy(&workers->changed) != 0 ||
        pthread_mutex_destroy(&workers->lock) != 0) abort();
#endif
    free(workers);
}
