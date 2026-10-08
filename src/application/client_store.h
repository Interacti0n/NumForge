#ifndef NUMFORGE_APPLICATION_CLIENT_STORE_H
#define NUMFORGE_APPLICATION_CLIENT_STORE_H

#include "session.h"

/* Private, transport-independent application storage. Zero-initialize and
 * destroy after use. IDs identify local clients, not authenticated users.
 * Fixed FIFO pools preserve the existing session/legacy-cache limits. */
#define NUMFORGE_APPLICATION_CLIENT_CAPACITY 8U
#define NUMFORGE_APPLICATION_CLIENT_ID_BYTES 32U

typedef struct ApplicationEvaluationCache
{
    CalculatorValue value;
    char expression[CALCULATOR_MAX_INPUT_BYTES + 1U];
    uint64_t revision;
} ApplicationEvaluationCache;

typedef struct ApplicationClientStore
{
    struct {
        char id[NUMFORGE_APPLICATION_CLIENT_ID_BYTES + 1U];
        CalculatorSession session;
    } sessions[NUMFORGE_APPLICATION_CLIENT_CAPACITY];
    struct {
        char id[NUMFORGE_APPLICATION_CLIENT_ID_BYTES + 1U];
        ApplicationEvaluationCache cache;
    } caches[NUMFORGE_APPLICATION_CLIENT_CAPACITY];
    size_t next_session;
    size_t next_cache;
} ApplicationClientStore;

/* Borrowed pointers remain valid until eviction or store destruction.
 * Lookup with create=false is read-only and never revives an expired session. */
CalculatorSession *application_client_session(ApplicationClientStore *store,
    const char *id, bool create);
/* Includes a bounded release tombstone, solely for retrying lifecycle calls. */
CalculatorSession *application_client_retained_session(ApplicationClientStore *store, const char *id);
ApplicationEvaluationCache *application_client_cache(ApplicationClientStore *store,
    const char *id);
void application_evaluation_cache_clear(ApplicationEvaluationCache *cache);
void application_client_store_destroy(ApplicationClientStore *store);

#endif
