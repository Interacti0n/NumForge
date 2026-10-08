#include "client_store.h"
#include <unity.h>
#include <stdio.h>
#include <stdlib.h>

static ApplicationClientStore store;
static ApplicationClientStore other;
static const char client[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

void setUp(void) {}
void tearDown(void)
{
    application_client_store_destroy(&store);
    application_client_store_destroy(&other);
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

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_stores_own_isolated_sessions_without_transport);
    RUN_TEST(test_session_eviction_is_fifo_and_independent_of_legacy_cache);
    RUN_TEST(test_invalid_ids_do_not_allocate_or_replace_sessions);
    RUN_TEST(test_lifecycle_preserves_typed_ans_and_rejects_old_mutations);
    RUN_TEST(test_conversion_history_is_independent_and_replay_uses_saved_value);
    return UNITY_END();
}
