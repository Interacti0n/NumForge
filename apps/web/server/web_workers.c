#include "apps/web/server/web_workers.h"
#include "apps/web/server/web_server.h"
#include <numforge/runtime.h>
#include <stdlib.h>
#ifdef _WIN32
#include <process.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

typedef struct WebConnection {
    NumForgeSocket socket;
    uint64_t accepted_ms;
} WebConnection;

struct NumForgeWebWorkers {
#ifdef _WIN32
    SRWLOCK lock;
    CONDITION_VARIABLE changed;
    HANDLE threads[NUMFORGE_WEB_MAX_WORKERS];
#else
    pthread_mutex_t lock;
    pthread_cond_t changed;
    pthread_t threads[NUMFORGE_WEB_MAX_WORKERS];
#endif
    WebConnection queue[NUMFORGE_WEB_MAX_QUEUE];
    size_t head, count, active, started, capacity;
    bool stopping;
    uint16_t port;
    const char *origin;
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
static void notify_workers(NumForgeWebWorkers *workers)
{
#ifdef _WIN32
    WakeAllConditionVariable(&workers->changed);
#else
    if (pthread_cond_broadcast(&workers->changed) != 0) abort();
#endif
}
static bool connection_expired(const WebConnection *connection, uint64_t now)
{
    return now < connection->accepted_ms || now - connection->accepted_ms >= NUMFORGE_WEB_QUEUE_TIMEOUT_MS;
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
        WebConnection connection = workers->queue[workers->head];
        workers->head = (workers->head + 1U) % workers->capacity;
        --workers->count; ++workers->active;
        unlock_workers(workers);

        uint64_t now = numforge_monotonic_ms();
        if (connection_expired(&connection, now))
            numforge_reject_connection(connection.socket);
        else numforge_handle_connection_before(connection.socket, workers->port, workers->origin,
            connection.accepted_ms + NUMFORGE_WEB_REQUEST_TIMEOUT_MS);
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
    if (!count || count > NUMFORGE_WEB_MAX_WORKERS || !capacity || capacity > NUMFORGE_WEB_MAX_QUEUE) return NULL;
    NumForgeWebWorkers *workers = calloc(1U, sizeof(*workers));
    if (!workers) return NULL;
    workers->capacity = capacity; workers->port = port; workers->origin = origin;
#ifdef _WIN32
    InitializeSRWLock(&workers->lock);
    InitializeConditionVariable(&workers->changed);
#else
    if (pthread_mutex_init(&workers->lock, NULL) != 0) { free(workers); return NULL; }
    if (pthread_cond_init(&workers->changed, NULL) != 0) {
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
    if (workers->started == count) return workers;
    numforge_web_workers_stop(workers);
    return NULL;
}

bool numforge_web_workers_submit(NumForgeWebWorkers *workers, NumForgeSocket socket, uint64_t accepted_ms)
{
    if (!workers || socket == NUMFORGE_INVALID_SOCKET || accepted_ms > UINT64_MAX - NUMFORGE_WEB_REQUEST_TIMEOUT_MS) return false;
    lock_workers(workers);
    bool accepted = !workers->stopping && workers->count < workers->capacity;
    if (accepted) {
        size_t tail = (workers->head + workers->count) % workers->capacity;
        workers->queue[tail] = (WebConnection){socket, accepted_ms};
        ++workers->count;
        notify_workers(workers);
    }
    unlock_workers(workers);
    return accepted;
}

void numforge_web_workers_expire(NumForgeWebWorkers *workers, uint64_t now)
{
    if (!workers) return;
    NumForgeSocket expired[NUMFORGE_WEB_MAX_QUEUE];
    size_t count = 0U;
    lock_workers(workers);
    while (workers->count && connection_expired(&workers->queue[workers->head], now)) {
        expired[count++] = workers->queue[workers->head].socket;
        workers->head = (workers->head + 1U) % workers->capacity;
        --workers->count;
    }
    unlock_workers(workers);
    for (size_t i = 0U; i < count; ++i) {
        numforge_reject_connection(expired[i]);
        numforge_close_socket(expired[i]);
    }
}

size_t numforge_web_workers_pending(NumForgeWebWorkers *workers, size_t *active)
{
    if (!workers) { if (active) *active = 0U; return 0U; }
    lock_workers(workers);
    size_t count = workers->count;
    if (active) *active = workers->active;
    unlock_workers(workers);
    return count;
}

void numforge_web_workers_stop(NumForgeWebWorkers *workers)
{
    if (!workers) return;
    lock_workers(workers);
    workers->stopping = true;
    NumForgeSocket queued[NUMFORGE_WEB_MAX_QUEUE];
    size_t count = workers->count;
    for (size_t i = 0U; i < count; ++i)
        queued[i] = workers->queue[(workers->head + i) % workers->capacity].socket;
    workers->count = 0U;
    notify_workers(workers);
    unlock_workers(workers);
    for (size_t i = 0U; i < count; ++i) {
        numforge_reject_connection(queued[i]);
        numforge_close_socket(queued[i]);
    }
    for (size_t i = 0U; i < workers->started; ++i) {
#ifdef _WIN32
        if (WaitForSingleObject(workers->threads[i], INFINITE) != WAIT_OBJECT_0 || !CloseHandle(workers->threads[i])) abort();
#else
        if (pthread_join(workers->threads[i], NULL) != 0) abort();
#endif
    }
#ifndef _WIN32
    if (pthread_cond_destroy(&workers->changed) != 0 || pthread_mutex_destroy(&workers->lock) != 0) abort();
#endif
    free(workers);
}
