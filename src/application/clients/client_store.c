#include "src/application/clients/client_store.h"
#include <string.h>
#ifdef _WIN32
#include <windows.h>
static SRWLOCK clients_lock = SRWLOCK_INIT;
static void lock_clients(void) { AcquireSRWLockExclusive(&clients_lock); }
static void unlock_clients(void) { ReleaseSRWLockExclusive(&clients_lock); }
#else
#include <pthread.h>
#include <stdlib.h>
static pthread_mutex_t clients_lock = PTHREAD_MUTEX_INITIALIZER;
static void lock_clients(void) { if (pthread_mutex_lock(&clients_lock) != 0) abort(); }
static void unlock_clients(void) { if (pthread_mutex_unlock(&clients_lock) != 0) abort(); }
#endif

/* One process-wide metadata lock preserves zero-initialized stores without a
 * fallible mutex lifecycle. It never covers use of a reserved session/cache. */

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
    const char *id, bool create, bool retained, size_t *slot)
{
    for (size_t i = 0U; i < NUMFORGE_APPLICATION_CLIENT_CAPACITY; i++)
        if (strcmp(store->sessions[i].id, id) == 0) {
            if (store->sessions[i].busy) return CALCULATOR_SESSION_BUSY;
            if (store->sessions[i].session.released && !retained) return CALCULATOR_SESSION_EXPIRED;
            *slot = i; return CALCULATOR_OK;
        }
    if (!create) return CALCULATOR_SESSION_EXPIRED;
    for (size_t i = 0U; i < NUMFORGE_APPLICATION_CLIENT_CAPACITY; i++) {
        size_t candidate = (store->next_session + i) % NUMFORGE_APPLICATION_CLIENT_CAPACITY;
        if (store->sessions[candidate].busy) continue;
        calculator_session_destroy(&store->sessions[candidate].session);
        memcpy(store->sessions[candidate].id, id, NUMFORGE_APPLICATION_CLIENT_ID_BYTES + 1U);
        store->next_session = (candidate + 1U) % NUMFORGE_APPLICATION_CLIENT_CAPACITY;
        *slot = candidate; return CALCULATOR_OK;
    }
    return CALCULATOR_SESSION_BUSY;
}
static CalculatorStatus cache_slot(ApplicationClientStore *store, const char *id, size_t *slot)
{
    for (size_t i = 0U; i < NUMFORGE_APPLICATION_CLIENT_CAPACITY; i++)
        if (strcmp(store->caches[i].id, id) == 0) {
            if (store->caches[i].busy) return CALCULATOR_SESSION_BUSY;
            *slot = i; return CALCULATOR_OK;
        }
    for (size_t i = 0U; i < NUMFORGE_APPLICATION_CLIENT_CAPACITY; i++) {
        size_t candidate = (store->next_cache + i) % NUMFORGE_APPLICATION_CLIENT_CAPACITY;
        if (store->caches[candidate].busy) continue;
        application_evaluation_cache_clear(&store->caches[candidate].cache);
        memcpy(store->caches[candidate].id, id, NUMFORGE_APPLICATION_CLIENT_ID_BYTES + 1U);
        store->next_cache = (candidate + 1U) % NUMFORGE_APPLICATION_CLIENT_CAPACITY;
        *slot = candidate; return CALCULATOR_OK;
    }
    return CALCULATOR_SESSION_BUSY;
}
CalculatorStatus application_client_acquire(ApplicationClientStore *store,
    const char *id, ApplicationClientAccess access, ApplicationClientLease *lease)
{
    if (!store || !id || !lease) return CALCULATOR_NULL_ARGUMENT;
    if (!valid_client_id(id) || lease->store || (access != APPLICATION_CLIENT_LIVE && access != APPLICATION_CLIENT_START &&
        access != APPLICATION_CLIENT_RETAINED && access != APPLICATION_CLIENT_CACHE))
        return CALCULATOR_INVALID_ARGUMENT;
    size_t slot = 0;
    lock_clients();
    CalculatorStatus status = access == APPLICATION_CLIENT_CACHE ? cache_slot(store, id, &slot) :
        session_slot(store, id, access == APPLICATION_CLIENT_START, access == APPLICATION_CLIENT_RETAINED, &slot);
    if (status != CALCULATOR_OK) { unlock_clients(); return status; }
    *lease = (ApplicationClientLease){store, slot, NULL, NULL};
    if (access == APPLICATION_CLIENT_CACHE) {
        store->caches[slot].busy = true; lease->cache = &store->caches[slot].cache;
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
    else lease->store->sessions[lease->slot].busy = false;
    memset(lease, 0, sizeof(*lease));
    unlock_clients();
}
CalculatorSession *application_client_session(ApplicationClientStore *store, const char *id, bool create)
{
    size_t slot = 0;
    return store && valid_client_id(id) && session_slot(store, id, create, false, &slot) == CALCULATOR_OK ?
        &store->sessions[slot].session : NULL;
}
CalculatorSession *application_client_retained_session(ApplicationClientStore *store, const char *id)
{
    size_t slot = 0;
    return store && valid_client_id(id) && session_slot(store, id, false, true, &slot) == CALCULATOR_OK ?
        &store->sessions[slot].session : NULL;
}
ApplicationEvaluationCache *application_client_cache(ApplicationClientStore *store, const char *id)
{
    size_t slot = 0;
    return store && valid_client_id(id) && cache_slot(store, id, &slot) == CALCULATOR_OK ? &store->caches[slot].cache : NULL;
}
bool application_client_store_destroy(ApplicationClientStore *store)
{
    if (!store) return true;
    lock_clients();
    for (size_t i = 0U; i < NUMFORGE_APPLICATION_CLIENT_CAPACITY; i++)
        if (store->sessions[i].busy || store->caches[i].busy) { unlock_clients(); return false; }
    for (size_t i = 0U; i < NUMFORGE_APPLICATION_CLIENT_CAPACITY; i++) {
        calculator_session_destroy(&store->sessions[i].session);
        application_evaluation_cache_clear(&store->caches[i].cache);
    }
    memset(store, 0, sizeof(*store));
    unlock_clients();
    return true;
}
