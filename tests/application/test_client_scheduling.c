#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#include "src/application/clients/client_store.h"
#include "apps/web/http/session_api.h"
#include "apps/web/http/web_api.h"
#include "src/runtime/phase_profile.h"
#include <numforge/runtime.h>
#include <unity.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
typedef volatile LONG Flag;
static void signal_flag(Flag *flag) { (void)InterlockedExchange(flag, 1); }
static bool flag_set(Flag *flag) { return InterlockedCompareExchange(flag, 0, 0) != 0; }
static void pause_thread(void) { Sleep(1); }
#else
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>
typedef _Atomic bool Flag;
static void signal_flag(Flag *flag) { atomic_store_explicit(flag, true, memory_order_release); }
static bool flag_set(Flag *flag) { return atomic_load_explicit(flag, memory_order_acquire); }
static void pause_thread(void) { const struct timespec interval = {0, 1000000L}; (void)nanosleep(&interval, NULL); }
#endif

static ApplicationClientStore store, other;
static const char client[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
static ApplicationClientLease held[NUMFORGE_APPLICATION_CLIENT_CAPACITY + 1U];

typedef struct Worker {
    ApplicationClientAccess access;
    unsigned int wait_ms;
    char id[33];
    const char *input, *expected, *target;
    uint64_t revision, observed;
    CalculatorStatus status;
    bool ok, hold, gate, started, joined, profile;
    Flag ready, proceed;
#ifdef _WIN32
    HANDLE thread;
#else
    pthread_t thread;
#endif
} Worker;
static Worker workers[NUMFORGE_APPLICATION_CLIENT_PENDING_LIMIT];

static bool await_flag(Flag *flag)
{
    uint64_t deadline = numforge_monotonic_ms() + 6000U;
    while (!flag_set(flag)) { if (numforge_monotonic_ms() >= deadline) return false; pause_thread(); }
    return true;
}
static bool await_pending(size_t count)
{
    uint64_t deadline = numforge_monotonic_ms() + 3000U;
    while (application_client_pending(&store) != count) {
        if (numforge_monotonic_ms() >= deadline) return false;
        pause_thread();
    }
    return true;
}
static Worker *prepare(size_t index, ApplicationClientAccess access)
{
    Worker *worker = &workers[index];
    memset(worker, 0, sizeof(*worker));
#ifndef _WIN32
    atomic_init(&worker->ready, false); atomic_init(&worker->proceed, false);
#endif
    memcpy(worker->id, client, sizeof(client));
    worker->access = access; worker->wait_ms = NUMFORGE_APPLICATION_CLIENT_MAX_WAIT_MS; worker->ok = true;
    return worker;
}
#ifdef _WIN32
static DWORD WINAPI run_worker(void *argument)
#else
static void *run_worker(void *argument)
#endif
{
    Worker *worker = argument;
    ApplicationClientLease lease = {0};
    if (worker->profile) {
        char *result = NULL;
        CalculatorError error;
        worker->ok = !numforge_profile_active();
        numforge_profile_reset();
        worker->ok = worker->ok && numforge_web_evaluate("200!", &result, &error) == CALCULATOR_OK;
        free(result);
        numforge_profile_stop();
        worker->ok = worker->ok && numforge_profile_complete() &&
            numforge_profile_seconds(NUMFORGE_PHASE_PARSE) > 0.0;
    } else if (worker->target) {
        char *response = NULL;
        worker->status = numforge_web_session_request(&store, "GET", worker->target, "", &response);
        worker->ok = response != NULL && strstr(response, worker->expected) != NULL;
        free(response);
    } else {
        worker->status = application_client_acquire_wait(&store, worker->id, worker->access, worker->wait_ms, &lease);
        if (worker->status == CALCULATOR_OK) {
            if (worker->gate) {
                signal_flag(&worker->ready);
                if (!await_flag(&worker->proceed)) worker->ok = false;
            }
            if (lease.cache) {
                worker->observed = lease.cache->revision;
                lease.cache->revision = worker->revision;
            } else if (worker->input) {
                CalculatorContext context; calculator_context_init(&context);
                CalculatorError error; char *text = NULL;
                if (lease.read_session) {
                    CalculatorValue value = {0};
                    const CalculatorSession *session = lease.read_session;
                    worker->observed = session->revision;
                    worker->ok = calculator_compute_value_with_variables(worker->input, &context,
                        calculator_session_answer(session), NULL, session->variables, session->variable_count,
                        &value, &error) == CALCULATOR_OK;
                    if (worker->ok) worker->ok = calculator_value_snapshot_text(&value, &text) == CALCULATOR_OK;
                    calculator_value_destroy(&value);
                } else if (!worker->expected) {
                    worker->ok = calculator_session_mutate(lease.session, worker->revision, CALCULATOR_SESSION_RELEASE) == CALCULATOR_OK;
                } else {
                    worker->status = calculator_session_compute(lease.session, worker->revision, true, worker->input,
                        &context, &text, &error, NULL);
                    worker->ok = worker->status == CALCULATOR_OK;
                }
                if (worker->expected) worker->ok = worker->ok && text != NULL && strcmp(text, worker->expected) == 0;
                free(text);
            }
            signal_flag(&worker->ready);
            if (worker->hold && !await_flag(&worker->proceed)) worker->ok = false;
        }
        application_client_release(&lease);
    }
    signal_flag(&worker->ready);
    return 0;
}
static void launch(Worker *worker)
{
#ifdef _WIN32
    worker->thread = CreateThread(NULL, 0, run_worker, worker, 0, NULL);
    worker->started = worker->thread != NULL;
#else
    worker->started = pthread_create(&worker->thread, NULL, run_worker, worker) == 0;
#endif
    TEST_ASSERT_TRUE(worker->started);
}
static void join_worker(Worker *worker)
{
    if (!worker->started || worker->joined) return;
#ifdef _WIN32
    if (WaitForSingleObject(worker->thread, INFINITE) != WAIT_OBJECT_0 || !CloseHandle(worker->thread)) abort();
#else
    if (pthread_join(worker->thread, NULL) != 0) abort();
#endif
    worker->joined = true;
}
/* Leave optional thread-local live-allocation tracking disabled: this suite
 * intentionally transfers ownership of session values between threads. */
void setUp(void) { memset(workers, 0, sizeof(workers)); }
void tearDown(void)
{
    for (size_t i = 0; i < NUMFORGE_APPLICATION_CLIENT_PENDING_LIMIT; ++i)
        if (workers[i].started) signal_flag(&workers[i].proceed);
    for (size_t i = 0; i <= NUMFORGE_APPLICATION_CLIENT_CAPACITY; ++i) application_client_release(&held[i]);
    for (size_t i = 0; i < NUMFORGE_APPLICATION_CLIENT_PENDING_LIMIT; ++i) join_worker(&workers[i]);
    TEST_ASSERT_EQUAL_UINT(0, application_client_pending(&store));
    TEST_ASSERT_TRUE(application_client_store_destroy(&store));
    TEST_ASSERT_TRUE(application_client_store_destroy(&other));
}
static void begin_session(bool read)
{
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_client_acquire(&store, client, APPLICATION_CLIENT_START, &held[0]));
    CalculatorContext context; calculator_context_init(&context);
    CalculatorError error; char *text = NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_session_compute(held[0].session, 1, true, "a=3", &context, &text, &error, NULL));
    free(text);
    if (read) {
        application_client_release(&held[0]);
        TEST_ASSERT_EQUAL(CALCULATOR_OK, application_client_acquire(&store, client, APPLICATION_CLIENT_READ, &held[0]));
    }
}
static void assert_workers(size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        join_worker(&workers[i]);
        TEST_ASSERT_EQUAL(CALCULATOR_OK, workers[i].status);
        TEST_ASSERT_TRUE(workers[i].ok);
    }
}
static void test_timeouts_and_validation_leave_the_store_and_lease_unchanged(void)
{
    begin_session(false);
    TEST_ASSERT_EQUAL(CALCULATOR_INVALID_ARGUMENT, application_client_acquire_wait(&store, client,
        APPLICATION_CLIENT_READ, NUMFORGE_APPLICATION_CLIENT_MAX_WAIT_MS + 1U, &held[1]));
    ApplicationClientStore *before = malloc(sizeof(*before)); TEST_ASSERT_NOT_NULL(before);
    memcpy(before, &store, sizeof(store));
    uint64_t started = numforge_monotonic_ms();
    CalculatorStatus status = application_client_acquire_wait(&store, client, APPLICATION_CLIENT_READ, 80, &held[1]);
    uint64_t elapsed = numforge_monotonic_ms() - started;
    bool unchanged = memcmp(before, &store, sizeof(store)) == 0; free(before);
    TEST_ASSERT_EQUAL(CALCULATOR_SESSION_BUSY, status); TEST_ASSERT_TRUE(unchanged);
    /* GetTickCount64 has a coarse tick; reject immediate failure without
     * requiring the clock to resolve the exact condition-variable timeout. */
    TEST_ASSERT_TRUE(elapsed >= 60U); TEST_ASSERT_NULL(held[1].store);
    TEST_ASSERT_EQUAL_UINT(0, application_client_pending(NULL));
    application_client_release(&held[0]);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_client_acquire_wait(&store, client, APPLICATION_CLIENT_READ, 0, &held[1]));
}
static void test_fifo_mutations_preserve_replay_and_read_snapshots(void)
{
    begin_session(true);
    const char *inputs[] = {"a=5", "a=5", "a+ans", "a+ans", "a=7"};
    const char *expected[] = {"5", "5", "10", "10", "7"};
    for (size_t i = 0; i < 5; ++i) {
        Worker *worker = prepare(i, i == 2 || i == 3 ? APPLICATION_CLIENT_READ : APPLICATION_CLIENT_LIVE);
        worker->input = inputs[i]; worker->expected = expected[i]; worker->revision = i == 4 ? 3 : 2;
        launch(worker); TEST_ASSERT_TRUE(await_pending(i + 1));
    }
    TEST_ASSERT_EQUAL(CALCULATOR_SESSION_BUSY, application_client_acquire(&store, client, APPLICATION_CLIENT_READ, &held[1]));
    application_client_expire(&store, numforge_monotonic_ms() + NUMFORGE_APPLICATION_CLIENT_IDLE_MS);
    TEST_ASSERT_FALSE(held[0].read_session->released);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_client_acquire_wait(&other, client, APPLICATION_CLIENT_START, 10, &held[1]));
    application_client_release(&held[1]);
    application_client_release(&held[0]); assert_workers(5);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_client_acquire(&store, client, APPLICATION_CLIENT_READ, &held[0]));
    TEST_ASSERT_EQUAL_UINT64(3, held[0].read_session->revision);
    TEST_ASSERT_EQUAL_UINT(3, held[0].read_session->count);
    TEST_ASSERT_EQUAL_UINT64(2, workers[2].observed); TEST_ASSERT_EQUAL_UINT64(2, workers[3].observed);
}
static void test_competing_clients_cannot_overcommit_the_shared_memory_quota(void)
{
    begin_session(false);
    size_t bytes = calculator_session_storage_bytes(held[0].session);
    application_client_release(&held[0]); TEST_ASSERT_TRUE(application_client_store_destroy(&store));
    ApplicationClientLimits limits = {bytes * 2U, bytes, NUMFORGE_APPLICATION_CLIENT_IDLE_MS};
    TEST_ASSERT_TRUE(application_client_store_configure(&store, &limits));
    for (size_t i = 0U; i < 2U; ++i) {
        Worker *worker = prepare(i, APPLICATION_CLIENT_START);
        (void)snprintf(worker->id, sizeof(worker->id), "%032u", (unsigned)i);
        worker->input = "a=3"; worker->expected = "3"; worker->revision = 1U; worker->gate = true;
        launch(worker); TEST_ASSERT_TRUE(await_flag(&worker->ready));
    }
    signal_flag(&workers[0].proceed); signal_flag(&workers[1].proceed);
    join_worker(&workers[0]); join_worker(&workers[1]);
    TEST_ASSERT_TRUE((workers[0].status == CALCULATOR_OK && workers[1].status == CALCULATOR_SESSION_MEMORY_LIMIT) ||
        (workers[1].status == CALCULATOR_OK && workers[0].status == CALCULATOR_SESSION_MEMORY_LIMIT));
    TEST_ASSERT_EQUAL_UINT(bytes, application_client_retained_bytes(&store));
}
static void test_queued_readers_share_a_phase_without_bypassing_the_next_writer(void)
{
    begin_session(false);
    for (size_t i = 0; i < 3; ++i) {
        Worker *worker = prepare(i, i < 2 ? APPLICATION_CLIENT_READ : APPLICATION_CLIENT_LIVE);
        worker->hold = i < 2; launch(worker); TEST_ASSERT_TRUE(await_pending(i + 1));
    }
    application_client_release(&held[0]);
    TEST_ASSERT_TRUE(await_flag(&workers[0].ready)); TEST_ASSERT_TRUE(await_flag(&workers[1].ready));
    TEST_ASSERT_FALSE(flag_set(&workers[2].ready));
    TEST_ASSERT_EQUAL_UINT(1, application_client_pending(&store));
    TEST_ASSERT_FALSE(application_client_store_destroy(&store));
    TEST_ASSERT_EQUAL(CALCULATOR_SESSION_BUSY, application_client_acquire(&store, client, APPLICATION_CLIENT_READ, &held[1]));
    signal_flag(&workers[0].proceed); join_worker(&workers[0]);
    TEST_ASSERT_FALSE(flag_set(&workers[2].ready));
    signal_flag(&workers[1].proceed); assert_workers(3);
}
static void test_expired_head_is_removed_and_notifications_do_not_extend_its_deadline(void)
{
    begin_session(true);
    Worker *writer = prepare(0, APPLICATION_CLIENT_LIVE); writer->wait_ms = 500;
    launch(writer); TEST_ASSERT_TRUE(await_pending(1));
    Worker *reader = prepare(1, APPLICATION_CLIENT_READ); reader->hold = true;
    launch(reader); TEST_ASSERT_TRUE(await_pending(2));
    uint64_t deadline = numforge_monotonic_ms() + 1000;
    do {
        TEST_ASSERT_EQUAL(CALCULATOR_OK, application_client_acquire(&other, client, APPLICATION_CLIENT_CACHE, &held[1]));
        application_client_release(&held[1]); pause_thread();
    } while (numforge_monotonic_ms() < deadline);
    TEST_ASSERT_TRUE(flag_set(&writer->ready)); TEST_ASSERT_TRUE(flag_set(&reader->ready));
    join_worker(writer); TEST_ASSERT_EQUAL(CALCULATOR_SESSION_BUSY, writer->status);
    TEST_ASSERT_EQUAL_UINT(0, application_client_pending(&store));
    signal_flag(&reader->proceed); join_worker(reader);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, reader->status);
    TEST_ASSERT_EQUAL_UINT64(1, held[0].read_session->revision);
}
static void test_release_tombstones_are_rechecked_after_waiting(void)
{
    begin_session(false);
    for (size_t i = 0; i < 3; ++i) {
        Worker *worker = prepare(i, i == 0 ? APPLICATION_CLIENT_LIVE : i == 1 ? APPLICATION_CLIENT_READ : APPLICATION_CLIENT_RETAINED);
        if (i == 2) { worker->input = "release"; worker->revision = 2; }
        launch(worker); TEST_ASSERT_TRUE(await_pending(i + 1));
    }
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_session_mutate(held[0].session, 2, CALCULATOR_SESSION_RELEASE));
    application_client_release(&held[0]);
    for (size_t i = 0; i < 3; ++i) join_worker(&workers[i]);
    TEST_ASSERT_EQUAL(CALCULATOR_SESSION_EXPIRED, workers[0].status);
    TEST_ASSERT_EQUAL(CALCULATOR_SESSION_EXPIRED, workers[1].status);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, workers[2].status); TEST_ASSERT_TRUE(workers[2].ok);
    TEST_ASSERT_EQUAL(CALCULATOR_SESSION_EXPIRED, application_client_acquire(&store, client, APPLICATION_CLIENT_START, &held[0]));
}
static void test_cache_waiters_remain_exclusive_and_ordered(void)
{
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_client_acquire(&store, client, APPLICATION_CLIENT_CACHE, &held[0]));
    for (size_t i = 0; i < 3; ++i) {
        Worker *worker = prepare(i, APPLICATION_CLIENT_CACHE); worker->revision = i + 1;
        launch(worker); TEST_ASSERT_TRUE(await_pending(i + 1));
    }
    application_client_release(&held[0]); assert_workers(3);
    for (size_t i = 0; i < 3; ++i) TEST_ASSERT_EQUAL_UINT64(i, workers[i].observed);
}
static void test_queue_limits_are_per_entry_and_per_store(void)
{
    for (size_t group = 0; group < 4; ++group) {
        char id[33]; (void)snprintf(id, sizeof(id), "%032u", (unsigned)group);
        TEST_ASSERT_EQUAL(CALCULATOR_OK, application_client_acquire(&store, id, APPLICATION_CLIENT_START, &held[group]));
        for (size_t i = 0; i < NUMFORGE_APPLICATION_CLIENT_PENDING_PER_ENTRY; ++i) {
            size_t index = group * NUMFORGE_APPLICATION_CLIENT_PENDING_PER_ENTRY + i;
            Worker *worker = prepare(index, APPLICATION_CLIENT_READ); memcpy(worker->id, id, sizeof(id));
            launch(worker); TEST_ASSERT_TRUE(await_pending(index + 1));
        }
    }
    uint64_t started = numforge_monotonic_ms();
    TEST_ASSERT_EQUAL(CALCULATOR_SESSION_BUSY, application_client_acquire_wait(&store, workers[0].id,
        APPLICATION_CLIENT_READ, NUMFORGE_APPLICATION_CLIENT_MAX_WAIT_MS, &held[5]));
    TEST_ASSERT_TRUE(numforge_monotonic_ms() - started < 1000U);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_client_acquire(&store, client, APPLICATION_CLIENT_CACHE, &held[4]));
    started = numforge_monotonic_ms();
    TEST_ASSERT_EQUAL(CALCULATOR_SESSION_BUSY, application_client_acquire_wait(&store, client,
        APPLICATION_CLIENT_CACHE, NUMFORGE_APPLICATION_CLIENT_MAX_WAIT_MS, &held[5]));
    TEST_ASSERT_TRUE(numforge_monotonic_ms() - started < 1000U);
    TEST_ASSERT_EQUAL_UINT(NUMFORGE_APPLICATION_CLIENT_PENDING_LIMIT, application_client_pending(&store));
    TEST_ASSERT_FALSE(application_client_store_destroy(&store));
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_client_acquire_wait(&other, client, APPLICATION_CLIENT_START, 10, &held[5]));
    for (size_t i = 0; i < 4; ++i) application_client_release(&held[i]);
    assert_workers(NUMFORGE_APPLICATION_CLIENT_PENDING_LIMIT);
}
static void test_http_session_reads_wait_for_the_updated_snapshot(void)
{
    begin_session(false);
    Worker *worker = prepare(0, APPLICATION_CLIENT_READ);
    worker->target = "/api/session/value?client=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa&name=a";
    worker->expected = "\"text\":\"9\"";
    launch(worker); TEST_ASSERT_TRUE(await_pending(1));
    CalculatorContext context; calculator_context_init(&context);
    CalculatorError error; char *text = NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_session_compute(held[0].session, 2, true, "a=9", &context, &text, &error, NULL));
    free(text); application_client_release(&held[0]); assert_workers(1);
}
#ifndef _WIN32
static void test_deferred_cancellation_unpins_the_waiting_entry(void)
{
    begin_session(true);
    Worker *worker = prepare(0, APPLICATION_CLIENT_LIVE);
    launch(worker); TEST_ASSERT_TRUE(await_pending(1));
    TEST_ASSERT_EQUAL_INT(0, pthread_cancel(worker->thread)); join_worker(worker);
    TEST_ASSERT_EQUAL_UINT(0, application_client_pending(&store));
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_client_acquire(&store, client, APPLICATION_CLIENT_READ, &held[1]));
}
#endif
static void test_request_profiles_are_thread_local(void)
{
    Worker *worker = prepare(0, APPLICATION_CLIENT_CACHE);
    worker->profile = true;
    numforge_profile_reset();
    NumForgeProfilePhase previous = numforge_profile_enter(NUMFORGE_PHASE_PARSE);
    launch(worker);
    join_worker(worker);
    TEST_ASSERT_TRUE(worker->ok);
    TEST_ASSERT_TRUE(numforge_profile_active());
    TEST_ASSERT_TRUE(numforge_profile_seconds(NUMFORGE_PHASE_PARSE) == 0.0);
    numforge_profile_leave(previous);
    numforge_profile_stop();
    TEST_ASSERT_TRUE(numforge_profile_seconds(NUMFORGE_PHASE_PARSE) > 0.0);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_request_profiles_are_thread_local);
    RUN_TEST(test_timeouts_and_validation_leave_the_store_and_lease_unchanged);
    RUN_TEST(test_fifo_mutations_preserve_replay_and_read_snapshots);
    RUN_TEST(test_competing_clients_cannot_overcommit_the_shared_memory_quota);
    RUN_TEST(test_queued_readers_share_a_phase_without_bypassing_the_next_writer);
    RUN_TEST(test_expired_head_is_removed_and_notifications_do_not_extend_its_deadline);
    RUN_TEST(test_release_tombstones_are_rechecked_after_waiting);
    RUN_TEST(test_cache_waiters_remain_exclusive_and_ordered);
    RUN_TEST(test_queue_limits_are_per_entry_and_per_store);
    RUN_TEST(test_http_session_reads_wait_for_the_updated_snapshot);
#ifndef _WIN32
    RUN_TEST(test_deferred_cancellation_unpins_the_waiting_entry);
#endif
    return UNITY_END();
}
