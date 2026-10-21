#include "src/application/clients/client_store.h"
#include <unity.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <sched.h>
#endif

static ApplicationClientStore store;
static ApplicationClientStore other;
static const char client[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
static ApplicationClientLease leases[NUMFORGE_APPLICATION_CLIENT_CAPACITY + 1];
static ApplicationClientLease cache_leases[NUMFORGE_APPLICATION_CLIENT_CAPACITY];

void setUp(void) {}
void tearDown(void)
{
    for (size_t i = 0; i < NUMFORGE_APPLICATION_CLIENT_CAPACITY + 1; ++i) application_client_release(&leases[i]);
    for (size_t i = 0; i < NUMFORGE_APPLICATION_CLIENT_CAPACITY; ++i) application_client_release(&cache_leases[i]);
    TEST_ASSERT_TRUE(application_client_store_destroy(&store));
    TEST_ASSERT_TRUE(application_client_store_destroy(&other));
}

static void compute(CalculatorSession *session, uint64_t revision, bool commit,
    const char *input, const char *expected)
{
    CalculatorContext context;
    CalculatorError error;
    char *text = NULL;
    calculator_context_init(&context);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_session_compute(session,
        revision, commit, input, &context, &text, &error, NULL));
    TEST_ASSERT_EQUAL_STRING(expected, text);
    free(text);
}

static void test_stores_own_isolated_sessions_without_transport(void)
{
    TEST_ASSERT_NULL(application_client_session(&store, client, false));
    CalculatorSession *first = application_client_session(&store, client, true);
    CalculatorSession *second = application_client_session(&other, client, true);
    TEST_ASSERT_NOT_NULL(first);
    TEST_ASSERT_NOT_NULL(second);
    TEST_ASSERT_TRUE(first != second);
    compute(first, 1U, true, "x=qty(3;\"m\")", "3 m");
    compute(second, 1U, true, "x=7", "7");
    TEST_ASSERT_EQUAL_PTR(first, application_client_session(&store, client, true));
    TEST_ASSERT_EQUAL_UINT(1, first->variable_count);
    TEST_ASSERT_EQUAL_UINT64(1U, first->revision);
    application_client_store_destroy(&store);
    TEST_ASSERT_NULL(application_client_session(&store, client, false));
    compute(second, 2U, false, "x", "7");
}

static void test_session_eviction_is_fifo_and_independent_of_legacy_cache(void)
{
    CalculatorSession *first = application_client_session(&store, client, true);
    compute(first, 1U, true, "x=5", "5");
    ApplicationEvaluationCache *cache = application_client_cache(&store, client);
    CalculatorContext context;
    CalculatorError error;
    calculator_context_init(&context);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_compute_value("qty(2;\"m\")",
        &context, &cache->value, &error));
    for (size_t i = 0U; i < NUMFORGE_APPLICATION_CLIENT_CAPACITY; i++) {
        char id[33];
        (void)snprintf(id, sizeof(id), "%032u", (unsigned)i);
        TEST_ASSERT_NOT_NULL(application_client_cache(&store, id));
    }
    TEST_ASSERT_EQUAL_PTR(first, application_client_session(&store, client, false));
    compute(first, 2U, false, "x", "5");
    for (size_t i = 0U; i < NUMFORGE_APPLICATION_CLIENT_CAPACITY; i++) {
        char id[33];
        (void)snprintf(id, sizeof(id), "%032u", (unsigned)i);
        TEST_ASSERT_NOT_NULL(application_client_session(&store, id, true));
    }
    TEST_ASSERT_NULL(application_client_session(&store, client, false));
    CalculatorSession *fresh = application_client_session(&store, client, true);
    TEST_ASSERT_EQUAL_UINT(0, fresh->variable_count);
    TEST_ASSERT_EQUAL_UINT(0, fresh->count);
}

static void test_invalid_ids_do_not_allocate_or_replace_sessions(void)
{
    CalculatorSession *first = application_client_session(&store, client, true);
    compute(first, 1U, true, "x=7", "7");
    TEST_ASSERT_NULL(application_client_session(&store, "", true));
    TEST_ASSERT_NULL(application_client_session(&store, "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", true));
    TEST_ASSERT_NULL(application_client_cache(&store, "bad"));
    TEST_ASSERT_NULL(application_client_session(NULL, client, true));
    TEST_ASSERT_NULL(application_client_cache(&store, NULL));
    TEST_ASSERT_EQUAL_PTR(first, application_client_session(&store, client, false));
    compute(first, 2U, false, "x", "7");
}

static void test_lifecycle_preserves_typed_ans_and_rejects_old_mutations(void)
{
    CalculatorSession *session = application_client_session(&store, client, true);
    compute(session, 1U, true, "x=qty(5;\"m\")", "5 m");
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_session_mutate(session, 2U, CALCULATOR_SESSION_CLEAR_HISTORY));
    TEST_ASSERT_EQUAL_UINT(0U, session->count);
    TEST_ASSERT_NOT_NULL(calculator_session_answer(session));
    TEST_ASSERT_TRUE(calculator_session_answer(session)->quantity);
    TEST_ASSERT_EQUAL_UINT(1U, session->variable_count);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_session_mutate(session, 2U, CALCULATOR_SESSION_CLEAR_HISTORY));
    TEST_ASSERT_EQUAL(CALCULATOR_STALE_REQUEST, calculator_session_mutate(session, 2U, CALCULATOR_SESSION_RESET));
    compute(session, 3U, true, "ans+x", "10 m");
    TEST_ASSERT_NULL(session->detached_answer.number);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_session_mutate(session, 4U, CALCULATOR_SESSION_RESET));
    TEST_ASSERT_NULL(calculator_session_answer(session));
    TEST_ASSERT_EQUAL_UINT(0U, session->variable_count);
    TEST_ASSERT_EQUAL_UINT64(4U, session->revision);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_session_mutate(session, 5U, CALCULATOR_SESSION_RELEASE));
    TEST_ASSERT_NULL(application_client_session(&store, client, false));
    TEST_ASSERT_NULL(application_client_session(&store, client, true));
    TEST_ASSERT_EQUAL_PTR(session, application_client_retained_session(&store, client));
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_session_mutate(session, 5U, CALCULATOR_SESSION_RELEASE));
    TEST_ASSERT_EQUAL(CALCULATOR_SESSION_EXPIRED, calculator_session_mutate(session, 6U, CALCULATOR_SESSION_RESET));
}

static void test_conversion_history_is_independent_and_replay_uses_saved_value(void)
{
    CalculatorSession *session = application_client_session(&store, client, true);
    CalculatorContext context; calculator_context_init(&context);
    CalculatorError error; const char *code = NULL;
    const ApplicationConversion *entry = NULL;
    compute(session, 1U, true, "x=1/3", "1/3");
    uint64_t random_state = session->random_state;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_conversion_confirm(session, 1U, "x", "km", "m", &context, &entry, &error, &code));
    TEST_ASSERT_EQUAL_STRING("1000/3", entry->display);
    TEST_ASSERT_TRUE(entry->value.quantity);
    TEST_ASSERT_EQUAL_INT(1, entry->value.dimensions[0]);
    TEST_ASSERT_EQUAL_UINT64(1U, session->revision);
    TEST_ASSERT_EQUAL_UINT64(random_state, session->random_state);
    compute(session, 2U, true, "x=9", "9");
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_conversion_confirm(session, 1U, "x", "km", "m", &context, &entry, &error, &code));
    TEST_ASSERT_EQUAL_STRING("1000/3", entry->display);
    TEST_ASSERT_EQUAL(CALCULATOR_STALE_REQUEST, application_conversion_confirm(session, 1U, "x+1", "km", "m", &context, &entry, &error, &code));
    TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT, application_conversion_confirm(session, 2U, "rand()", "km", "m", &context, &entry, &error, &code));
    TEST_ASSERT_EQUAL_UINT(1U, session->conversion_count);
    TEST_ASSERT_EQUAL_UINT64(1U, session->conversion_revision);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_conversion_clear(session, 2U));
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_conversion_clear(session, 2U));
    TEST_ASSERT_EQUAL_UINT(0U, session->conversion_count);
    TEST_ASSERT_EQUAL_UINT(2U, session->count);
    TEST_ASSERT_EQUAL(CALCULATOR_STALE_REQUEST, application_conversion_confirm(session, 1U, "x", "km", "m", &context, &entry, &error, &code));
}

static void test_lease_pins_session_and_excludes_a_second_operation(void)
{
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_client_acquire(&store, client, APPLICATION_CLIENT_START, &leases[0]));
    CalculatorSession *saved = leases[0].session;
    compute(saved, 1, true, "x=5", "5");
    for (size_t i = 0; i < 3 * NUMFORGE_APPLICATION_CLIENT_CAPACITY; ++i) {
        char id[33]; (void)snprintf(id, sizeof(id), "%032u", (unsigned)i);
        TEST_ASSERT_NOT_NULL(application_client_session(&store, id, true));
    }
    compute(saved, 2, false, "x", "5");
    for (int mode = APPLICATION_CLIENT_LIVE; mode <= APPLICATION_CLIENT_RETAINED; ++mode)
        TEST_ASSERT_EQUAL(CALCULATOR_SESSION_BUSY, application_client_acquire(&store, client, (ApplicationClientAccess)mode, &leases[1]));
    TEST_ASSERT_NULL(leases[1].store);
    TEST_ASSERT_NULL(application_client_session(&store, client, false));
    TEST_ASSERT_NULL(application_client_retained_session(&store, client));
    TEST_ASSERT_FALSE(application_client_store_destroy(&store));
    compute(saved, 3, false, "x", "5");
    application_client_release(&leases[0]); application_client_release(&leases[0]);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_client_acquire(&store, client, APPLICATION_CLIENT_LIVE, &leases[1]));
    TEST_ASSERT_EQUAL_PTR(saved, leases[1].session); compute(leases[1].session, 4, false, "x", "5");
}
static void test_all_held_pools_refuse_replacement_without_mutation(void)
{
    for (size_t i = 0; i < NUMFORGE_APPLICATION_CLIENT_CAPACITY; ++i) {
        char id[33]; (void)snprintf(id, sizeof(id), "%032u", (unsigned)i);
        TEST_ASSERT_EQUAL(CALCULATOR_OK, application_client_acquire(&store, id, APPLICATION_CLIENT_START, &leases[i]));
        if (i % 2 != 0) {
            application_client_release(&leases[i]);
            TEST_ASSERT_EQUAL(CALCULATOR_OK, application_client_acquire(&store, id, APPLICATION_CLIENT_READ, &leases[i]));
        }
        TEST_ASSERT_EQUAL(CALCULATOR_OK, application_client_acquire(&store, id, APPLICATION_CLIENT_CACHE, &cache_leases[i]));
    }
    ApplicationClientStore *before = malloc(sizeof(*before)); TEST_ASSERT_NOT_NULL(before);
    memcpy(before, &store, sizeof(store));
    ApplicationClientLease blocked = {0};
    TEST_ASSERT_EQUAL(CALCULATOR_SESSION_BUSY, application_client_acquire(&store, client, APPLICATION_CLIENT_START, &blocked));
    TEST_ASSERT_EQUAL(CALCULATOR_SESSION_BUSY, application_client_acquire(&store, client, APPLICATION_CLIENT_CACHE, &blocked));
    TEST_ASSERT_FALSE(application_client_store_destroy(&store));
    TEST_ASSERT_EQUAL_MEMORY(before, &store, sizeof(store)); free(before);
    TEST_ASSERT_NULL(blocked.store); TEST_ASSERT_NULL(blocked.session); TEST_ASSERT_NULL(blocked.cache); TEST_ASSERT_NULL(blocked.read_session);
    application_client_release(&leases[3]);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_client_acquire(&store, client, APPLICATION_CLIENT_START, &leases[3]));
    TEST_ASSERT_EQUAL_UINT(3, leases[3].slot);
    /* Cache reservations remain independent of the session pool. */
    TEST_ASSERT_EQUAL(CALCULATOR_SESSION_BUSY, application_client_acquire(&store, client, APPLICATION_CLIENT_CACHE, &blocked));
    application_client_release(&cache_leases[4]);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_client_acquire(&store, client, APPLICATION_CLIENT_CACHE, &cache_leases[4]));
    TEST_ASSERT_EQUAL_UINT(4, cache_leases[4].slot);
}
static void test_lease_validation_and_release_tombstone(void)
{
    TEST_ASSERT_EQUAL(CALCULATOR_NULL_ARGUMENT, application_client_acquire(NULL, client, APPLICATION_CLIENT_START, &leases[0]));
    TEST_ASSERT_EQUAL(CALCULATOR_NULL_ARGUMENT, application_client_acquire(&store, NULL, APPLICATION_CLIENT_START, &leases[0]));
    TEST_ASSERT_EQUAL(CALCULATOR_NULL_ARGUMENT, application_client_acquire(&store, client, APPLICATION_CLIENT_START, NULL));
    TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT, application_client_acquire(&store, "bad", APPLICATION_CLIENT_START, &leases[0]));
    TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT, application_client_acquire(&store, client, (ApplicationClientAccess)99, &leases[0]));
    TEST_ASSERT_EQUAL(CALCULATOR_SESSION_EXPIRED, application_client_acquire(&store, client, APPLICATION_CLIENT_LIVE, &leases[0]));
    TEST_ASSERT_EQUAL(CALCULATOR_SESSION_EXPIRED, application_client_acquire(&store, client, APPLICATION_CLIENT_READ, &leases[0]));
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_client_acquire(&store, client, APPLICATION_CLIENT_START, &leases[0]));
    TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT, application_client_acquire(&other, client, APPLICATION_CLIENT_START, &leases[0]));
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_session_mutate(leases[0].session, 1, CALCULATOR_SESSION_RELEASE));
    TEST_ASSERT_EQUAL(CALCULATOR_SESSION_BUSY, application_client_acquire(&store, client, APPLICATION_CLIENT_RETAINED, &leases[1]));
    application_client_release(&leases[0]);
    TEST_ASSERT_EQUAL(CALCULATOR_SESSION_EXPIRED, application_client_acquire(&store, client, APPLICATION_CLIENT_START, &leases[0]));
    TEST_ASSERT_EQUAL(CALCULATOR_SESSION_EXPIRED, application_client_acquire(&store, client, APPLICATION_CLIENT_LIVE, &leases[0]));
    TEST_ASSERT_EQUAL(CALCULATOR_SESSION_EXPIRED, application_client_acquire(&store, client, APPLICATION_CLIENT_READ, &leases[0]));
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_client_acquire(&store, client, APPLICATION_CLIENT_RETAINED, &leases[0]));
    TEST_ASSERT_TRUE(leases[0].session->released);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_session_mutate(leases[0].session, 1, CALCULATOR_SESSION_RELEASE));
    application_client_release(NULL); TEST_ASSERT_TRUE(application_client_store_destroy(NULL));
}
static void test_shared_readers_keep_the_session_immutable_and_pinned(void)
{
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_client_acquire(&store, client,
        APPLICATION_CLIENT_START, &leases[0]));
    compute(leases[0].session, 1, true, "x=5", "5");
    application_client_release(&leases[0]);
    for (size_t i = 0; i < 4; ++i) {
        TEST_ASSERT_EQUAL(CALCULATOR_OK, application_client_acquire(&store, client,
            APPLICATION_CLIENT_READ, &leases[i]));
        TEST_ASSERT_NOT_NULL(leases[i].read_session);
        TEST_ASSERT_NULL(leases[i].session); TEST_ASSERT_NULL(leases[i].cache);
        TEST_ASSERT_EQUAL_PTR(leases[0].read_session, leases[i].read_session);
        TEST_ASSERT_EQUAL_UINT64(1, leases[i].read_session->revision);
    }
    for (size_t i = 0; i < 4; ++i) {
        TEST_ASSERT_EQUAL(CALCULATOR_SESSION_BUSY, application_client_acquire(&store, client,
            APPLICATION_CLIENT_LIVE, &leases[4]));
        TEST_ASSERT_FALSE(application_client_store_destroy(&store));
        application_client_release(&leases[i]);
    }
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_client_acquire(&store, client,
        APPLICATION_CLIENT_LIVE, &leases[4]));
    compute(leases[4].session, 2, true, "x=7", "7");
    TEST_ASSERT_EQUAL(CALCULATOR_SESSION_BUSY, application_client_acquire(&store, client,
        APPLICATION_CLIENT_READ, &leases[0]));
    application_client_release(&leases[4]);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_client_acquire(&store, client,
        APPLICATION_CLIENT_READ, &leases[0]));
    TEST_ASSERT_EQUAL_UINT64(2, leases[0].read_session->revision);
}

typedef struct ClientWorker {
    bool ok;
    bool cache;
    bool reader;
} ClientWorker;

#ifdef _WIN32
static DWORD WINAPI update_client(void *argument)
#else
static void *update_client(void *argument)
#endif
{
    ClientWorker *worker = argument;
    worker->ok = true;
    for (size_t completed = 0; completed < 2000;) {
        ApplicationClientLease lease = {0};
        CalculatorStatus status = application_client_acquire(&store, client,
            worker->reader ? APPLICATION_CLIENT_READ :
            worker->cache ? APPLICATION_CLIENT_CACHE : APPLICATION_CLIENT_LIVE, &lease);
        if (status == CALCULATOR_SESSION_BUSY) continue;
        if (status != CALCULATOR_OK) { worker->ok = false; break; }
        if (worker->reader) {
            uint64_t revision = lease.read_session->revision;
#ifdef _WIN32
            (void)SwitchToThread();
#else
            (void)sched_yield();
#endif
            if (completed % 100 == 0) {
                CalculatorContext context; calculator_context_init(&context);
                CalculatorValue value = {0}; CalculatorError error; char *text = NULL;
                CalculatorStatus calculated = calculator_compute_value_with_variables("a+ans", &context,
                    calculator_session_answer(lease.read_session), NULL, lease.read_session->variables,
                    lease.read_session->variable_count, &value, &error);
                if (calculated == CALCULATOR_OK) calculated = calculator_value_snapshot_text(&value, &text);
                if (calculated != CALCULATOR_OK || text == NULL || strcmp(text, "10") != 0) worker->ok = false;
                free(text); calculator_value_destroy(&value);
            }
            if (lease.read_session->revision != revision) worker->ok = false;
        } else if (worker->cache) ++lease.cache->revision;
        else ++lease.session->revision;
        /* A separate, permanently pinned entry also blocks whole-store teardown. */
        if (application_client_store_destroy(&store)) worker->ok = false;
        application_client_release(&lease);
        if (!worker->ok) break;
        ++completed;
    }
    return 0;
}

static void test_concurrent_leases_preserve_updates_and_reject_destruction(void)
{
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_client_acquire(&store, client,
        APPLICATION_CLIENT_START, &leases[0]));
    compute(leases[0].session, 1, true, "a=5", "5");
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_client_acquire(&store,
        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb", APPLICATION_CLIENT_START, &leases[NUMFORGE_APPLICATION_CLIENT_CAPACITY]));
    ClientWorker workers[12] = {{0}};
    size_t started = 0;
#ifdef _WIN32
    HANDLE threads[12];
#else
    pthread_t threads[12];
#endif
    for (size_t i = 0; i < 12; ++i) {
        workers[i].cache = i % 2 != 0;
        workers[i].reader = i >= 8;
#ifdef _WIN32
        threads[i] = CreateThread(NULL, 0, update_client, &workers[i], 0, NULL);
        if (!threads[i]) break;
#else
        if (pthread_create(&threads[i], NULL, update_client, &workers[i]) != 0) break;
#endif
        ++started;
    }
    /* Keep session readers/writers blocked until all workers have been created. */
    application_client_release(&leases[0]);
    bool joined = true;
    for (size_t i = 0; i < started; ++i) {
#ifdef _WIN32
        if (WaitForSingleObject(threads[i], INFINITE) != WAIT_OBJECT_0) abort();
        joined &= CloseHandle(threads[i]) != 0;
#else
        if (pthread_join(threads[i], NULL) != 0) abort();
#endif
    }
    TEST_ASSERT_TRUE(joined);
    TEST_ASSERT_EQUAL_UINT(12, started);
    for (size_t i = 0; i < started; ++i) TEST_ASSERT_TRUE(workers[i].ok);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_client_acquire(&store, client,
        APPLICATION_CLIENT_LIVE, &leases[1]));
    TEST_ASSERT_EQUAL_UINT64(8001, leases[1].session->revision);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_client_acquire(&store, client,
        APPLICATION_CLIENT_CACHE, &cache_leases[0]));
    TEST_ASSERT_EQUAL_UINT64(8000, cache_leases[0].cache->revision);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_shared_readers_keep_the_session_immutable_and_pinned);
    RUN_TEST(test_concurrent_leases_preserve_updates_and_reject_destruction);
    RUN_TEST(test_lease_pins_session_and_excludes_a_second_operation);
    RUN_TEST(test_all_held_pools_refuse_replacement_without_mutation);
    RUN_TEST(test_lease_validation_and_release_tombstone);
    RUN_TEST(test_stores_own_isolated_sessions_without_transport);
    RUN_TEST(test_session_eviction_is_fifo_and_independent_of_legacy_cache);
    RUN_TEST(test_invalid_ids_do_not_allocate_or_replace_sessions);
    RUN_TEST(test_lifecycle_preserves_typed_ans_and_rejects_old_mutations);
    RUN_TEST(test_conversion_history_is_independent_and_replay_uses_saved_value);
    return UNITY_END();
}
