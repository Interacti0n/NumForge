#include "src/application/clients/client_store.h"
#include "apps/web/http/web_api.h"
#include "src/runtime/numforge_alloc.h"
#include <unity.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ApplicationClientStore store;
static ApplicationClientLease leases[4];
static CalculatorSession sample;
static CalculatorValue probe;
static char *preview_text, *confirmation_text;
static CalculatorContext context;
static const char alice[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
static const char bob[] = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
static bool tracking;
#endif

void setUp(void) { calculator_context_init(&context); }
void tearDown(void)
{
    for (size_t i = 0; i < 4; ++i) application_client_release(&leases[i]);
    TEST_ASSERT_TRUE(application_client_store_destroy(&store));
    calculator_session_destroy(&sample); calculator_value_destroy(&probe);
    free(preview_text); free(confirmation_text); preview_text = confirmation_text = NULL;
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
    if (tracking) {
        TEST_ASSERT_TRUE(numforge_alloc_stats_complete());
        TEST_ASSERT_EQUAL_UINT(0, numforge_alloc_stats_live());
        TEST_ASSERT_TRUE(numforge_alloc_stats_track(false)); tracking = false;
    }
#endif
}
static CalculatorStatus compute(CalculatorSession *session, uint64_t revision, bool commit, const char *input)
{
    char *text = NULL; CalculatorError error;
    CalculatorStatus status = calculator_session_compute(session, revision, commit, input, &context, &text, &error, NULL);
    if (status != CALCULATOR_OK) TEST_ASSERT_NULL(text);
    free(text); return status;
}
static size_t sample_bytes(const char *input)
{
    calculator_session_destroy(&sample);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, compute(&sample, 1, true, input));
    return calculator_session_storage_bytes(&sample);
}
static void configure(size_t client, size_t total, uint64_t idle)
{
    ApplicationClientLimits limits = {client, total, idle};
    TEST_ASSERT_TRUE(application_client_store_configure(&store, &limits));
}
static CalculatorSession *start(size_t index, const char *id)
{
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_client_acquire(&store, id, APPLICATION_CLIENT_START, &leases[index]));
    return leases[index].session;
}
static CalculatorStatus cache_compute(ApplicationEvaluationCache *cache, uint64_t revision)
{
    char *text = NULL; CalculatorError error;
    CalculatorStatus status = numforge_web_evaluate_cached(cache, revision, "2", context.output_scale,
        context.angle_unit, &text, &error, NULL);
    if (status != CALCULATOR_OK) TEST_ASSERT_NULL(text);
    free(text); return status;
}
static void test_opaque_storage_queries_include_all_owned_numeric_allocations(void)
{
    TEST_ASSERT_EQUAL_UINT(0, calculator_value_storage_bytes(NULL));
    TEST_ASSERT_EQUAL_UINT(0, calculator_session_storage_bytes(NULL));
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
    TEST_ASSERT_TRUE(numforge_alloc_stats_track(true)); tracking = true;
#endif
    const char *inputs[] = {"2^4096", "1/3", "sqrt(2)", "sqrt(-2)", "complex(1/3;2/7)", "qty(5;\"m\")"};
    for (size_t i = 0; i < sizeof(inputs) / sizeof(*inputs); ++i) {
        CalculatorError error;
        TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_compute_value(inputs[i], &context, &probe, &error));
        TEST_ASSERT_TRUE(calculator_value_storage_bytes(&probe) > 0U);
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
        TEST_ASSERT_EQUAL_UINT(numforge_alloc_stats_live(), calculator_value_storage_bytes(&probe));
#endif
        calculator_value_destroy(&probe);
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
        TEST_ASSERT_EQUAL_UINT(0, numforge_alloc_stats_live());
#endif
    }
    TEST_ASSERT_EQUAL(CALCULATOR_OK, compute(&sample, 1, true, "x=1/3"));
    TEST_ASSERT_EQUAL(CALCULATOR_OK, compute(&sample, 2, false, "sqrt(2)"));
    const ApplicationConversion *entry = NULL; CalculatorError error; const char *code = NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_conversion_confirm(&sample, 1, "x", "m", "cm", &context, &entry, &error, &code));
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
    TEST_ASSERT_EQUAL_UINT(numforge_alloc_stats_live(), calculator_session_storage_bytes(&sample));
#endif
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_session_mutate(&sample, 3, CALCULATOR_SESSION_CLEAR_HISTORY));
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
    TEST_ASSERT_EQUAL_UINT(numforge_alloc_stats_live(), calculator_session_storage_bytes(&sample));
#endif
}
static void test_limit_rejection_preserves_state_revision_and_replay(void)
{
    size_t bytes = sample_bytes("x=7"); configure(bytes, bytes * 4U, 1000);
    CalculatorSession *session = start(0, alice);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, compute(session, 1, true, "x=7"));
    TEST_ASSERT_EQUAL_UINT(bytes, application_client_retained_bytes(&store));
    CalculatorSession before = *session;
    TEST_ASSERT_EQUAL(CALCULATOR_SESSION_MEMORY_LIMIT, compute(session, 2, true, "x=8"));
    TEST_ASSERT_EQUAL_MEMORY(&before, session, sizeof(before));
    TEST_ASSERT_EQUAL(CALCULATOR_SESSION_MEMORY_LIMIT, compute(session, 2, false, "rand()"));
    TEST_ASSERT_EQUAL_MEMORY(&before, session, sizeof(before));
    TEST_ASSERT_EQUAL_UINT(bytes, application_client_retained_bytes(&store));
    TEST_ASSERT_EQUAL(CALCULATOR_OK, compute(session, 1, true, "x=7"));
    TEST_ASSERT_EQUAL_UINT(1, session->count);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_session_mutate(session, 2, CALCULATOR_SESSION_CLEAR_HISTORY));
    TEST_ASSERT_TRUE(application_client_retained_bytes(&store) < bytes);
    TEST_ASSERT_NOT_NULL(calculator_session_answer(session));
    TEST_ASSERT_EQUAL(CALCULATOR_OK, compute(session, 3, true, "x=8"));
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_session_mutate(session, 4, CALCULATOR_SESSION_RESET));
    TEST_ASSERT_EQUAL_UINT(0, application_client_retained_bytes(&store));
    before = *session;
    TEST_ASSERT_EQUAL(CALCULATOR_SESSION_MEMORY_LIMIT, compute(session, 5, true, "x=2^10000"));
    TEST_ASSERT_EQUAL_MEMORY(&before, session, sizeof(before));
}
static void test_global_limit_can_be_retried_after_another_client_releases_memory(void)
{
    size_t bytes = sample_bytes("7"); configure(bytes * 2U, bytes, 1000);
    CalculatorSession *first = start(0, alice), *second = start(1, bob);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, compute(first, 1, true, "7"));
    TEST_ASSERT_EQUAL(CALCULATOR_SESSION_MEMORY_LIMIT, compute(second, 1, true, "7"));
    TEST_ASSERT_EQUAL_UINT64(0, second->revision); TEST_ASSERT_FALSE(second->random_initialized);
    TEST_ASSERT_EQUAL_UINT(bytes, application_client_retained_bytes(&store));
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_session_mutate(first, 2, CALCULATOR_SESSION_RELEASE));
    TEST_ASSERT_EQUAL_UINT(0, application_client_retained_bytes(&store));
    TEST_ASSERT_EQUAL(CALCULATOR_OK, compute(second, 1, true, "7"));
    TEST_ASSERT_EQUAL_UINT(bytes, application_client_retained_bytes(&store));
}
static void test_session_and_cache_share_the_client_quota_and_cache_retry_identifier(void)
{
    size_t session_bytes = sample_bytes("x=7"); CalculatorError error;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_compute_value("2", &context, &probe, &error));
    size_t cache_bytes = calculator_value_storage_bytes(&probe);
    configure(session_bytes + cache_bytes - 1U, 4U * (session_bytes + cache_bytes), 1000);
    CalculatorSession *session = start(0, alice);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, compute(session, 1, true, "x=7"));
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_client_acquire(&store, alice, APPLICATION_CLIENT_CACHE, &leases[1]));
    TEST_ASSERT_EQUAL(CALCULATOR_SESSION_MEMORY_LIMIT, cache_compute(leases[1].cache, 1));
    TEST_ASSERT_EQUAL_UINT64(0, leases[1].cache->revision); TEST_ASSERT_NULL(leases[1].cache->value.number);
    TEST_ASSERT_EQUAL_UINT(session_bytes, application_client_retained_bytes(&store));
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_session_mutate(session, 2, CALCULATOR_SESSION_RESET));
    TEST_ASSERT_EQUAL(CALCULATOR_OK, cache_compute(leases[1].cache, 1));
    TEST_ASSERT_EQUAL_UINT(cache_bytes, application_client_retained_bytes(&store));
    application_evaluation_cache_clear(leases[1].cache);
    TEST_ASSERT_EQUAL_UINT(0, application_client_retained_bytes(&store));
}
static void test_random_preview_survives_quota_rejection_and_commits_the_original_draw_once(void)
{
    sample.random_initialized = true; sample.random_state = 42U;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, compute(&sample, 1, false, "rand()"));
    size_t preview_bytes = calculator_session_storage_bytes(&sample), other_bytes = sample_bytes("2^1024");
    configure(2U * (preview_bytes + other_bytes), preview_bytes + other_bytes, 1000);
    CalculatorSession *session = start(0, alice), *other = start(1, bob);
    session->random_initialized = true; session->random_state = 42U;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, compute(other, 1, true, "2^1024"));
    CalculatorError error;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_session_compute(session, 1, false, "rand()", &context, &preview_text, &error, NULL));
    CalculatorSession before = *session;
    TEST_ASSERT_EQUAL(CALCULATOR_SESSION_MEMORY_LIMIT, calculator_session_compute(session, 2, true, "rand()", &context, &confirmation_text, &error, NULL));
    TEST_ASSERT_NULL(confirmation_text); TEST_ASSERT_EQUAL_MEMORY(&before, session, sizeof(before));
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_session_mutate(other, 2, CALCULATOR_SESSION_RELEASE));
    TEST_ASSERT_EQUAL(CALCULATOR_OK, calculator_session_compute(session, 2, true, "rand()", &context, &confirmation_text, &error, NULL));
    TEST_ASSERT_EQUAL_STRING(preview_text, confirmation_text);
    TEST_ASSERT_EQUAL_UINT64(before.preview_random_state, session->random_state);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, compute(session, 2, true, "rand()"));
    TEST_ASSERT_EQUAL_UINT(1, session->count);
    TEST_ASSERT_EQUAL_UINT64(before.preview_random_state, session->random_state);
}
static void test_expiry_pins_readers_and_resets_idle_time_after_release(void)
{
    configure(NUMFORGE_APPLICATION_CLIENT_RETAINED_LIMIT, NUMFORGE_APPLICATION_STORE_RETAINED_LIMIT, 1000);
    CalculatorSession *session = start(0, alice);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, compute(session, 1, true, "x=7"));
    size_t slot = leases[0].slot, bytes = calculator_session_storage_bytes(session);
    application_client_release(&leases[0]); uint64_t idle = store.sessions[slot].last_used;
    application_client_expire(&store, idle + 999U);
    TEST_ASSERT_FALSE(store.sessions[slot].session.released);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_client_acquire(&store, alice, APPLICATION_CLIENT_READ, &leases[0]));
    application_client_expire(&store, idle + 10000U);
    TEST_ASSERT_EQUAL_UINT(bytes, application_client_retained_bytes(&store));
    TEST_ASSERT_NOT_NULL(calculator_session_answer(leases[0].read_session));
    application_client_release(&leases[0]); idle = store.sessions[slot].last_used;
    application_client_expire(&store, idle + 1000U);
    TEST_ASSERT_EQUAL_UINT(0, application_client_retained_bytes(&store));
    TEST_ASSERT_EQUAL(CALCULATOR_SESSION_EXPIRED, application_client_acquire(&store, alice, APPLICATION_CLIENT_READ, &leases[0]));
    TEST_ASSERT_EQUAL(CALCULATOR_SESSION_EXPIRED, application_client_acquire(&store, alice, APPLICATION_CLIENT_START, &leases[0]));
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_client_acquire(&store, alice, APPLICATION_CLIENT_RETAINED, &leases[0]));
    TEST_ASSERT_TRUE(leases[0].session->released);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_client_acquire(&store, bob, APPLICATION_CLIENT_CACHE, &leases[1]));
    TEST_ASSERT_EQUAL(CALCULATOR_OK, cache_compute(leases[1].cache, 1)); slot = leases[1].slot;
    application_client_expire(&store, numforge_monotonic_ms() + 10000U);
    TEST_ASSERT_TRUE(application_client_retained_bytes(&store) > 0U);
    application_client_release(&leases[1]); idle = store.caches[slot].last_used;
    application_client_expire(&store, idle + 1000U);
    TEST_ASSERT_EQUAL_UINT(0, application_client_retained_bytes(&store));
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_client_acquire(&store, bob, APPLICATION_CLIENT_CACHE, &leases[1]));
    TEST_ASSERT_EQUAL_UINT64(0, leases[1].cache->revision);
}
static void test_conversion_quota_rejects_before_confirmation_and_clear_releases_storage(void)
{
    const ApplicationConversion *entry = NULL; CalculatorError error; const char *code = NULL;
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_conversion_confirm(&sample, 1, "7", "m", "cm", &context, &entry, &error, &code));
    size_t bytes = calculator_session_storage_bytes(&sample); configure(bytes, bytes * 2U, 1000);
    CalculatorSession *session = start(0, alice);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_conversion_confirm(session, 1, "7", "m", "cm", &context, &entry, &error, &code));
    TEST_ASSERT_EQUAL_UINT(bytes, application_client_retained_bytes(&store));
    CalculatorSession before = *session;
    TEST_ASSERT_EQUAL(CALCULATOR_SESSION_MEMORY_LIMIT, application_conversion_confirm(session, 2, "8", "m", "cm", &context, &entry, &error, &code));
    TEST_ASSERT_EQUAL_STRING("session_memory_limit", code); TEST_ASSERT_NULL(entry);
    TEST_ASSERT_EQUAL_MEMORY(&before, session, sizeof(before));
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_conversion_confirm(session, 1, "7", "m", "cm", &context, &entry, &error, &code));
    TEST_ASSERT_EQUAL_UINT(1, session->conversion_count);
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_conversion_clear(session, 2));
    TEST_ASSERT_EQUAL_UINT(0, application_client_retained_bytes(&store));
    TEST_ASSERT_EQUAL(CALCULATOR_OK, application_conversion_confirm(session, 3, "7", "m", "cm", &context, &entry, &error, &code));
    TEST_ASSERT_EQUAL_UINT(bytes, application_client_retained_bytes(&store));
}
static void test_configuration_requires_empty_storage_and_invalid_limits_do_not_change_it(void)
{
    ApplicationClientLimits limits = {0, 1000, 1000};
    TEST_ASSERT_FALSE(application_client_store_configure(&store, &limits));
    limits.client_bytes = 1000; limits.idle_ms = 0;
    TEST_ASSERT_FALSE(application_client_store_configure(&store, &limits));
    configure(1000, 2000, 1000); start(0, alice);
    limits = (ApplicationClientLimits){500, 1000, 1000};
    TEST_ASSERT_FALSE(application_client_store_configure(&store, &limits));
    TEST_ASSERT_EQUAL_UINT(1000, store.limits.client_bytes);
    TEST_ASSERT_EQUAL_UINT(0, application_client_retained_bytes(NULL));
}
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_opaque_storage_queries_include_all_owned_numeric_allocations);
    RUN_TEST(test_limit_rejection_preserves_state_revision_and_replay);
    RUN_TEST(test_global_limit_can_be_retried_after_another_client_releases_memory);
    RUN_TEST(test_session_and_cache_share_the_client_quota_and_cache_retry_identifier);
    RUN_TEST(test_random_preview_survives_quota_rejection_and_commits_the_original_draw_once);
    RUN_TEST(test_expiry_pins_readers_and_resets_idle_time_after_release);
    RUN_TEST(test_conversion_quota_rejects_before_confirmation_and_clear_releases_storage);
    RUN_TEST(test_configuration_requires_empty_storage_and_invalid_limits_do_not_change_it);
    return UNITY_END();
}
