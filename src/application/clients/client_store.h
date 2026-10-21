#ifndef NUMFORGE_APPLICATION_CLIENT_STORE_H
#define NUMFORGE_APPLICATION_CLIENT_STORE_H

#include "src/application/sessions/session.h"

/* Private, transport-independent application storage. Zero-initialize and
 * destroy after use; do not copy or move a live store. IDs identify local clients,
 * not authenticated users.
 * Replacement scans from the FIFO cursor, skipping held/queued slots. */
#define NUMFORGE_APPLICATION_CLIENT_CAPACITY 64U
#define NUMFORGE_APPLICATION_CLIENT_IDLE_MS UINT64_C(1800000)
#define NUMFORGE_APPLICATION_CLIENT_RETAINED_LIMIT (8U * 1024U * 1024U)
#define NUMFORGE_APPLICATION_STORE_RETAINED_LIMIT (128U * 1024U * 1024U)
#define NUMFORGE_APPLICATION_CLIENT_ID_BYTES 32U
#define NUMFORGE_APPLICATION_CLIENT_PENDING_PER_ENTRY 8U
#define NUMFORGE_APPLICATION_CLIENT_PENDING_LIMIT 32U
#define NUMFORGE_APPLICATION_CLIENT_WAIT_MS 250U
#define NUMFORGE_APPLICATION_CLIENT_MAX_WAIT_MS 5000U

/* Nodes live on waiting callers' stacks and are accessed only under the lock. */
struct ApplicationClientWaiter;
typedef struct ApplicationClientQueue {
    struct ApplicationClientWaiter *head;
    struct ApplicationClientWaiter *tail;
    size_t count;
} ApplicationClientQueue;

typedef struct ApplicationEvaluationCache
{
    ApplicationRetention retention;
    CalculatorValue value;
    char expression[CALCULATOR_MAX_INPUT_BYTES + 1U];
    uint64_t revision;
} ApplicationEvaluationCache;

typedef struct ApplicationClientLimits {
    size_t client_bytes; /* Combined session and legacy cache for one ID. */
    size_t total_bytes;  /* Both pools, excluding fixed store/allocator overhead. */
    uint64_t idle_ms;
} ApplicationClientLimits;
typedef struct ApplicationClientMemory {
    struct ApplicationClientStore *store;
    const char *id;
    size_t bytes;
} ApplicationClientMemory;

typedef struct ApplicationClientStore
{
    struct {
        char id[NUMFORGE_APPLICATION_CLIENT_ID_BYTES + 1U];
        CalculatorSession session;
        bool busy;
        size_t readers;
        ApplicationClientQueue queue;
        ApplicationClientMemory memory;
        uint64_t last_used;
    } sessions[NUMFORGE_APPLICATION_CLIENT_CAPACITY];
    struct {
        char id[NUMFORGE_APPLICATION_CLIENT_ID_BYTES + 1U];
        ApplicationEvaluationCache cache;
        bool busy;
        ApplicationClientQueue queue;
        ApplicationClientMemory memory;
        uint64_t last_used;
    } caches[NUMFORGE_APPLICATION_CLIENT_CAPACITY];
    size_t next_session;
    size_t next_cache;
    size_t pending;
    size_t retained_bytes;
    ApplicationClientLimits limits; /* All zero selects the defaults above. */
} ApplicationClientStore;

/* Only on an empty store before workers start. Positive limits may tighten the
 * default byte caps; false leaves the store unchanged. */
bool application_client_store_configure(ApplicationClientStore *store, const ApplicationClientLimits *limits);
/* Monotonic sweep: held/read/queued entries are pinned. Expired session IDs keep
 * bounded tombstones; START cannot revive one while its slot remains retained. */
void application_client_expire(ApplicationClientStore *store, uint64_t now);
size_t application_client_retained_bytes(ApplicationClientStore *store);

typedef enum ApplicationClientAccess {
    APPLICATION_CLIENT_LIVE,
    APPLICATION_CLIENT_START,
    APPLICATION_CLIENT_RETAINED,
    APPLICATION_CLIENT_CACHE,
    APPLICATION_CLIENT_READ
} ApplicationClientAccess;

/* Request-scoped reservation. READ shares a live, immutable session with other
 * readers; all other modes are exclusive. Zero-initialize, do not copy, and release
 * exactly once after the last use of session/cache (release of an empty lease is
 * safe). A held slot cannot be evicted or destroyed; nonblocking acquisition
 * returns SESSION_BUSY on conflict. Pending requests also
 * pin entries and prevent newer immediate acquisitions from bypassing them.
 * If every replacement slot is held or queued, creation
 * returns SESSION_BUSY without modifying the store or lease.
 *
 * Acquire/release/destroy synchronize metadata internally. A lease and its
 * payload belong to one thread until release, except immutable shared reads;
 * never share the lease itself. READ exposes only read_session, never session.
 * Different reserved entries can be used concurrently. This does not make the
 * HTTP server multithreaded or synchronize other application state.
 */
typedef struct ApplicationClientLease {
    ApplicationClientStore *store;
    size_t slot;
    CalculatorSession *session;
    ApplicationEvaluationCache *cache;
    const CalculatorSession *read_session;
} ApplicationClientLease;
CalculatorStatus application_client_acquire(ApplicationClientStore *store,
    const char *id, ApplicationClientAccess access, ApplicationClientLease *lease);
/* Wait at most wait_ms (0 is nonblocking; values above MAX_WAIT_MS are invalid).
 * Only conflicts on existing entries queue; a full replacement pool fails at once.
 * FIFO writers cannot be bypassed; consecutive queued READs may share a lease
 * phase. Per-entry/store limits and the original monotonic deadline apply even
 * after spurious wakeups. Timeout/full queues return SESSION_BUSY with an empty
 * lease and unchanged payload/revisions. Queued RELEASE tombstones are rechecked.
 * OS scheduling/metadata-lock delays are not a hard real-time guarantee.
 * Workers use cooperative shutdown; never forcibly terminate a lease owner.
 */
CalculatorStatus application_client_acquire_wait(ApplicationClientStore *store,
    const char *id, ApplicationClientAccess access, unsigned int wait_ms, ApplicationClientLease *lease);
void application_client_release(ApplicationClientLease *lease);
/* Diagnostic snapshot of queued calls, not active leases or an admission promise. */
size_t application_client_pending(ApplicationClientStore *store);

/* Serial setup helpers: exclude all concurrent store access while using these
 * functions or their borrowed pointers. Request handlers must use leases instead.
 * Borrowed pointers remain valid until eviction or store destruction.
 * Lookup with create=false is read-only and never revives an expired session. */
CalculatorSession *application_client_session(ApplicationClientStore *store,
    const char *id, bool create);
/* Includes a bounded release tombstone, solely for retrying lifecycle calls. */
CalculatorSession *application_client_retained_session(ApplicationClientStore *store, const char *id);
ApplicationEvaluationCache *application_client_cache(ApplicationClientStore *store,
    const char *id);
void application_evaluation_cache_clear(ApplicationEvaluationCache *cache);
/* Refuse destruction while any lease is held or a caller is queued; false leaves
 * the entire store unchanged. Otherwise clear/reset; NULL is accepted as success.
 * The caller must stop/join workers before destroying their store. */
bool application_client_store_destroy(ApplicationClientStore *store);

#endif
