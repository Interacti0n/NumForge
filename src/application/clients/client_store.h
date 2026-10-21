#ifndef NUMFORGE_APPLICATION_CLIENT_STORE_H
#define NUMFORGE_APPLICATION_CLIENT_STORE_H

#include "src/application/sessions/session.h"

/* Private, transport-independent application storage. Zero-initialize and
 * destroy after use. IDs identify local clients, not authenticated users.
 * Fixed pools preserve the existing limits. Replacement scans from the FIFO
 * cursor, skipping reserved slots. */
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
        bool busy;
    } sessions[NUMFORGE_APPLICATION_CLIENT_CAPACITY];
    struct {
        char id[NUMFORGE_APPLICATION_CLIENT_ID_BYTES + 1U];
        ApplicationEvaluationCache cache;
        bool busy;
    } caches[NUMFORGE_APPLICATION_CLIENT_CAPACITY];
    size_t next_session;
    size_t next_cache;
} ApplicationClientStore;

typedef enum ApplicationClientAccess {
    APPLICATION_CLIENT_LIVE,
    APPLICATION_CLIENT_START,
    APPLICATION_CLIENT_RETAINED,
    APPLICATION_CLIENT_CACHE
} ApplicationClientAccess;

/* Request-scoped exclusive reservation. Zero-initialize, do not copy, and release
 * exactly once after the last use of session/cache (release of an empty lease is
 * safe). A held slot cannot be evicted or destroyed; another acquisition of the
 * same entry returns SESSION_BUSY. If every replacement slot is held, creation
 * returns SESSION_BUSY without modifying the store or lease.
 *
 * Bookkeeping is not internally synchronized yet: calls require external serial
 * access. This lifetime foundation alone does not make the server multithreaded.
 */
typedef struct ApplicationClientLease {
    ApplicationClientStore *store;
    size_t slot;
    CalculatorSession *session;
    ApplicationEvaluationCache *cache;
} ApplicationClientLease;
CalculatorStatus application_client_acquire(ApplicationClientStore *store,
    const char *id, ApplicationClientAccess access, ApplicationClientLease *lease);
void application_client_release(ApplicationClientLease *lease);

/* Serial setup helpers; request handlers must use leases instead.
 * Borrowed pointers remain valid until eviction or store destruction.
 * Lookup with create=false is read-only and never revives an expired session. */
CalculatorSession *application_client_session(ApplicationClientStore *store,
    const char *id, bool create);
/* Includes a bounded release tombstone, solely for retrying lifecycle calls. */
CalculatorSession *application_client_retained_session(ApplicationClientStore *store, const char *id);
ApplicationEvaluationCache *application_client_cache(ApplicationClientStore *store,
    const char *id);
void application_evaluation_cache_clear(ApplicationEvaluationCache *cache);
/* Refuse destruction while any lease is held; false leaves the entire store
 * unchanged. Otherwise clear all values and reset; NULL is accepted as success.
 * The caller must stop/join future workers before destroying their store. */
bool application_client_store_destroy(ApplicationClientStore *store);

#endif
