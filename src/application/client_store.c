#include "client_store.h"
#include <string.h>

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

CalculatorSession *application_client_session(ApplicationClientStore *store,
    const char *id, bool create)
{
    if (store == NULL || !valid_client_id(id)) return NULL;
    for (size_t i = 0U; i < NUMFORGE_APPLICATION_CLIENT_CAPACITY; i++)
        if (strcmp(store->sessions[i].id, id) == 0) return &store->sessions[i].session;
    if (!create) return NULL;
    size_t slot = store->next_session;
    store->next_session = (slot + 1U) % NUMFORGE_APPLICATION_CLIENT_CAPACITY;
    calculator_session_destroy(&store->sessions[slot].session);
    memcpy(store->sessions[slot].id, id, NUMFORGE_APPLICATION_CLIENT_ID_BYTES + 1U);
    return &store->sessions[slot].session;
}

ApplicationEvaluationCache *application_client_cache(ApplicationClientStore *store,
    const char *id)
{
    if (store == NULL || !valid_client_id(id)) return NULL;
    for (size_t i = 0U; i < NUMFORGE_APPLICATION_CLIENT_CAPACITY; i++)
        if (strcmp(store->caches[i].id, id) == 0) return &store->caches[i].cache;
    size_t slot = store->next_cache;
    store->next_cache = (slot + 1U) % NUMFORGE_APPLICATION_CLIENT_CAPACITY;
    application_evaluation_cache_clear(&store->caches[slot].cache);
    memcpy(store->caches[slot].id, id, NUMFORGE_APPLICATION_CLIENT_ID_BYTES + 1U);
    return &store->caches[slot].cache;
}

void application_client_store_destroy(ApplicationClientStore *store)
{
    if (store == NULL) return;
    for (size_t i = 0U; i < NUMFORGE_APPLICATION_CLIENT_CAPACITY; i++) {
        calculator_session_destroy(&store->sessions[i].session);
        application_evaluation_cache_clear(&store->caches[i].cache);
    }
    memset(store, 0, sizeof(*store));
}
