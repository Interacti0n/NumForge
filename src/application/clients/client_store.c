#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
#define _DARWIN_C_SOURCE
#elif !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#include "src/application/clients/client_store.h"
#include <numforge/runtime.h>
#include <string.h>
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
static SRWLOCK clients_lock = SRWLOCK_INIT;
static CONDITION_VARIABLE clients_changed = CONDITION_VARIABLE_INIT;
static void lock_clients(void) { AcquireSRWLockExclusive(&clients_lock); }
static void unlock_clients(void) { ReleaseSRWLockExclusive(&clients_lock); }
static void notify_clients(void) { WakeAllConditionVariable(&clients_changed); }
static bool wait_clients(unsigned int milliseconds)
{
    if (SleepConditionVariableSRW(&clients_changed, &clients_lock, milliseconds, 0)) return true;
    if (GetLastError() != ERROR_TIMEOUT) abort();
    return false;
}
#else
#include <pthread.h>
#include <errno.h>
#include <time.h>
static pthread_mutex_t clients_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t clients_changed;
static pthread_once_t clients_once = PTHREAD_ONCE_INIT;
static void initialize_clients(void)
{
    pthread_condattr_t attributes;
    if (pthread_condattr_init(&attributes) != 0) abort();
#ifndef __APPLE__
    if (pthread_condattr_setclock(&attributes, CLOCK_MONOTONIC) != 0) abort();
#endif
    if (pthread_cond_init(&clients_changed, &attributes) != 0 || pthread_condattr_destroy(&attributes) != 0) abort();
}
static void lock_clients(void)
{
    if (pthread_once(&clients_once, initialize_clients) != 0 || pthread_mutex_lock(&clients_lock) != 0) abort();
}
static void unlock_clients(void) { if (pthread_mutex_unlock(&clients_lock) != 0) abort(); }
static void notify_clients(void) { if (pthread_cond_broadcast(&clients_changed) != 0) abort(); }
static bool wait_clients(unsigned int milliseconds)
{
    struct timespec deadline = {0};
#ifndef __APPLE__
    if (clock_gettime(CLOCK_MONOTONIC, &deadline) != 0) return false;
#endif
    deadline.tv_sec += (time_t)(milliseconds / 1000U);
    deadline.tv_nsec += (long)(milliseconds % 1000U) * 1000000L;
    deadline.tv_sec += deadline.tv_nsec / 1000000000L;
    deadline.tv_nsec %= 1000000000L;
#ifdef __APPLE__
    int status = pthread_cond_timedwait_relative_np(&clients_changed, &clients_lock, &deadline);
#else
    int status = pthread_cond_timedwait(&clients_changed, &clients_lock, &deadline);
#endif
    if (status != 0 && status != ETIMEDOUT) abort();
    return status == 0;
}
#endif

/* One process-wide metadata lock preserves zero-initialized stores without a
 * per-store mutex lifecycle. Computation/serialization never hold this lock. */

typedef struct ApplicationClientWaiter {
    struct ApplicationClientWaiter *next;
    bool read;
} ApplicationClientWaiter;

static bool queue_ready(const ApplicationClientQueue *queue,
    const ApplicationClientWaiter *waiter, bool read)
{
    if (waiter == NULL) return queue->head == NULL;
    for (const ApplicationClientWaiter *before = queue->head; before != waiter; before = before->next)
        if (!read || !before->read) return false;
    return true;
}

static bool valid_client_id(const char *id)
{
    if (id == NULL || strlen(id) != NUMFORGE_APPLICATION_CLIENT_ID_BYTES) return false;
    for (size_t i = 0U; i < NUMFORGE_APPLICATION_CLIENT_ID_BYTES; i++)
        if (!((id[i] >= '0' && id[i] <= '9') || (id[i] >= 'a' && id[i] <= 'f'))) return false;
    return true;
}

void application_evaluation_cache_clear(ApplicationEvaluationCache *cache)
{
    if (cache == NULL) return;
    calculator_value_destroy(&cache->value);
    memset(cache, 0, sizeof(*cache));
}

static CalculatorStatus session_slot(ApplicationClientStore *store,
    const char *id, ApplicationClientAccess access, size_t *slot, const ApplicationClientWaiter *waiter)
{
    for (size_t i = 0U; i < NUMFORGE_APPLICATION_CLIENT_CAPACITY; i++)
        if (strcmp(store->sessions[i].id, id) == 0) {
            *slot = i;
            if (store->sessions[i].busy || (access != APPLICATION_CLIENT_READ && store->sessions[i].readers) ||
                store->sessions[i].readers == SIZE_MAX ||
                !queue_ready(&store->sessions[i].queue, waiter, access == APPLICATION_CLIENT_READ)) return CALCULATOR_SESSION_BUSY;
            if (store->sessions[i].session.released && access != APPLICATION_CLIENT_RETAINED) return CALCULATOR_SESSION_EXPIRED;
            return CALCULATOR_OK;
        }
    if (access != APPLICATION_CLIENT_START) return CALCULATOR_SESSION_EXPIRED;
    for (size_t i = 0U; i < NUMFORGE_APPLICATION_CLIENT_CAPACITY; i++) {
        size_t candidate = (store->next_session + i) % NUMFORGE_APPLICATION_CLIENT_CAPACITY;
        if (store->sessions[candidate].busy || store->sessions[candidate].readers || store->sessions[candidate].queue.count) continue;
        calculator_session_destroy(&store->sessions[candidate].session);
        memcpy(store->sessions[candidate].id, id, NUMFORGE_APPLICATION_CLIENT_ID_BYTES + 1U);
        store->next_session = (candidate + 1U) % NUMFORGE_APPLICATION_CLIENT_CAPACITY;
        *slot = candidate; return CALCULATOR_OK;
    }
    return CALCULATOR_SESSION_BUSY;
}
static CalculatorStatus cache_slot(ApplicationClientStore *store, const char *id, size_t *slot, const ApplicationClientWaiter *waiter)
{
    for (size_t i = 0U; i < NUMFORGE_APPLICATION_CLIENT_CAPACITY; i++)
        if (strcmp(store->caches[i].id, id) == 0) {
            *slot = i;
            if (store->caches[i].busy || !queue_ready(&store->caches[i].queue, waiter, false)) return CALCULATOR_SESSION_BUSY;
            return CALCULATOR_OK;
        }
    for (size_t i = 0U; i < NUMFORGE_APPLICATION_CLIENT_CAPACITY; i++) {
        size_t candidate = (store->next_cache + i) % NUMFORGE_APPLICATION_CLIENT_CAPACITY;
        if (store->caches[candidate].busy || store->caches[candidate].queue.count) continue;
        application_evaluation_cache_clear(&store->caches[candidate].cache);
        memcpy(store->caches[candidate].id, id, NUMFORGE_APPLICATION_CLIENT_ID_BYTES + 1U);
        store->next_cache = (candidate + 1U) % NUMFORGE_APPLICATION_CLIENT_CAPACITY;
        *slot = candidate; return CALCULATOR_OK;
    }
    return CALCULATOR_SESSION_BUSY;
}
typedef struct PendingClient {
    ApplicationClientStore *store;
    ApplicationClientQueue *queue;
    ApplicationClientWaiter node;
} PendingClient;

static void remove_pending(PendingClient *pending)
{
    ApplicationClientQueue *queue = pending->queue;
    ApplicationClientWaiter **link = &queue->head, *previous = NULL;
    while (*link != &pending->node) { previous = *link; link = &previous->next; }
    *link = pending->node.next;
    if (queue->tail == &pending->node) queue->tail = previous;
    --queue->count; --pending->store->pending;
    notify_clients();
}
#ifndef _WIN32
/* Deferred pthread cancellation reacquires the mutex before this cleanup. */
static void cancel_pending(void *argument)
{
    remove_pending(argument);
    unlock_clients();
}
#endif

static CalculatorStatus await_slot(ApplicationClientStore *store, const char *id,
    ApplicationClientAccess access, size_t *slot, unsigned int wait_ms)
{
    ApplicationClientQueue *queue = access == APPLICATION_CLIENT_CACHE ?
        &store->caches[*slot].queue : &store->sessions[*slot].queue;
    uint64_t started = numforge_monotonic_ms();
    if (queue->count == NUMFORGE_APPLICATION_CLIENT_PENDING_PER_ENTRY ||
        store->pending == NUMFORGE_APPLICATION_CLIENT_PENDING_LIMIT || started > UINT64_MAX - wait_ms)
        return CALCULATOR_SESSION_BUSY;
    uint64_t deadline = started + wait_ms;
    PendingClient pending = {.store = store, .queue = queue, .node = {.read = access == APPLICATION_CLIENT_READ}};
    if (queue->tail) queue->tail->next = &pending.node;
    else queue->head = &pending.node;
    queue->tail = &pending.node;
    ++queue->count; ++store->pending;
    CalculatorStatus status = CALCULATOR_SESSION_BUSY;
#ifndef _WIN32
    pthread_cleanup_push(cancel_pending, &pending);
#endif
    for (;;) {
        uint64_t now = numforge_monotonic_ms();
        if (now >= deadline) break;
        status = access == APPLICATION_CLIENT_CACHE ? cache_slot(store, id, slot, &pending.node) :
            session_slot(store, id, access, slot, &pending.node);
        if (status != CALCULATOR_SESSION_BUSY || !wait_clients((unsigned int)(deadline - now))) break;
    }
    remove_pending(&pending);
#ifndef _WIN32
    pthread_cleanup_pop(0);
#endif
    return status;
}

CalculatorStatus application_client_acquire(ApplicationClientStore *store,
    const char *id, ApplicationClientAccess access, ApplicationClientLease *lease)
{
    return application_client_acquire_wait(store, id, access, 0U, lease);
}
CalculatorStatus application_client_acquire_wait(ApplicationClientStore *store,
    const char *id, ApplicationClientAccess access, unsigned int wait_ms, ApplicationClientLease *lease)
{
    if (!store || !id || !lease) return CALCULATOR_NULL_ARGUMENT;
    if (!valid_client_id(id) || lease->store || wait_ms > NUMFORGE_APPLICATION_CLIENT_MAX_WAIT_MS ||
        (access != APPLICATION_CLIENT_LIVE && access != APPLICATION_CLIENT_START &&
        access != APPLICATION_CLIENT_RETAINED && access != APPLICATION_CLIENT_CACHE && access != APPLICATION_CLIENT_READ))
        return CALCULATOR_INVALID_ARGUMENT;
    size_t slot = NUMFORGE_APPLICATION_CLIENT_CAPACITY;
    lock_clients();
    CalculatorStatus status = access == APPLICATION_CLIENT_CACHE ? cache_slot(store, id, &slot, NULL) :
        session_slot(store, id, access, &slot, NULL);
    if (status == CALCULATOR_SESSION_BUSY && wait_ms && slot < NUMFORGE_APPLICATION_CLIENT_CAPACITY)
        status = await_slot(store, id, access, &slot, wait_ms);
    if (status != CALCULATOR_OK) { unlock_clients(); return status; }
    *lease = (ApplicationClientLease){.store = store, .slot = slot};
    if (access == APPLICATION_CLIENT_CACHE) {
        store->caches[slot].busy = true; lease->cache = &store->caches[slot].cache;
    } else if (access == APPLICATION_CLIENT_READ) {
        ++store->sessions[slot].readers; lease->read_session = &store->sessions[slot].session;
    } else {
        store->sessions[slot].busy = true; lease->session = &store->sessions[slot].session;
    }
    unlock_clients();
    return CALCULATOR_OK;
}
void application_client_release(ApplicationClientLease *lease)
{
    if (!lease || !lease->store) return;
    lock_clients();
    if (lease->cache) lease->store->caches[lease->slot].busy = false;
    else if (lease->read_session) --lease->store->sessions[lease->slot].readers;
    else lease->store->sessions[lease->slot].busy = false;
    memset(lease, 0, sizeof(*lease));
    notify_clients();
    unlock_clients();
}
CalculatorSession *application_client_session(ApplicationClientStore *store, const char *id, bool create)
{
    size_t slot = 0;
    return store && valid_client_id(id) && session_slot(store, id,
        create ? APPLICATION_CLIENT_START : APPLICATION_CLIENT_LIVE, &slot, NULL) == CALCULATOR_OK ?
        &store->sessions[slot].session : NULL;
}
CalculatorSession *application_client_retained_session(ApplicationClientStore *store, const char *id)
{
    size_t slot = 0;
    return store && valid_client_id(id) && session_slot(store, id, APPLICATION_CLIENT_RETAINED, &slot, NULL) == CALCULATOR_OK ?
        &store->sessions[slot].session : NULL;
}
ApplicationEvaluationCache *application_client_cache(ApplicationClientStore *store, const char *id)
{
    size_t slot = 0;
    return store && valid_client_id(id) && cache_slot(store, id, &slot, NULL) == CALCULATOR_OK ? &store->caches[slot].cache : NULL;
}
size_t application_client_pending(ApplicationClientStore *store)
{
    if (!store) return 0U;
    lock_clients();
    size_t count = store->pending;
    unlock_clients();
    return count;
}

bool application_client_store_destroy(ApplicationClientStore *store)
{
    if (!store) return true;
    lock_clients();
    if (store->pending) { unlock_clients(); return false; }
    for (size_t i = 0U; i < NUMFORGE_APPLICATION_CLIENT_CAPACITY; i++)
        if (store->sessions[i].busy || store->sessions[i].readers || store->caches[i].busy) { unlock_clients(); return false; }
    for (size_t i = 0U; i < NUMFORGE_APPLICATION_CLIENT_CAPACITY; i++) {
        calculator_session_destroy(&store->sessions[i].session);
        application_evaluation_cache_clear(&store->caches[i].cache);
    }
    memset(store, 0, sizeof(*store));
    unlock_clients();
    return true;
}
