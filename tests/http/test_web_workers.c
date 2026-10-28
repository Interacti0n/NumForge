#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#include "apps/web/server/web_workers.h"
#include "apps/web/server/web_server.h"
#include <numforge/runtime.h>
#include <unity.h>
#include <string.h>
#ifndef _WIN32
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#endif

static NumForgeWebWorkers *pool;
static struct { NumForgeSocket server, peer; } connections[5];
static const char request_tail[] = "ET /missing HTTP/1.1\r\nHost: localhost\r\n\r\n";
static const char calculate[] = "POST /api/evaluate HTTP/1.1\r\nHost: localhost\r\nContent-Length: 3\r\n\r\n2+2";

static void pause_thread(void)
{
#ifdef _WIN32
    Sleep(1);
#else
    const struct timespec interval = {0, 1000000L}; (void)nanosleep(&interval, NULL);
#endif
}
static bool connect_pair(size_t index)
{
    NumForgeSocket listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == NUMFORGE_INVALID_SOCKET) return false;
    struct sockaddr_in address; memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET; address.sin_addr.s_addr = htonl(UINT32_C(0x7f000001)); /* 127.0.0.1 */
#ifdef _WIN32
    int length = sizeof(address);
#else
    socklen_t length = sizeof(address);
#endif
    bool ok = bind(listener, (const struct sockaddr *)&address, sizeof(address)) == 0 &&
        getsockname(listener, (struct sockaddr *)&address, &length) == 0 && listen(listener, 1) == 0;
    if (ok) {
        connections[index].peer = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        ok = connections[index].peer != NUMFORGE_INVALID_SOCKET &&
            connect(connections[index].peer, (const struct sockaddr *)&address, sizeof(address)) == 0;
    }
    if (ok) {
        connections[index].server = accept(listener, NULL, NULL);
        ok = connections[index].server != NUMFORGE_INVALID_SOCKET;
    }
    numforge_close_socket(listener);
    if (!ok) return false;
#ifdef _WIN32
    u_long nonblocking = 1UL; DWORD timeout = 4000U;
    return ioctlsocket(connections[index].server, FIONBIO, &nonblocking) == 0 &&
        setsockopt(connections[index].peer, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout)) == 0;
#else
    int flags = fcntl(connections[index].server, F_GETFL, 0);
    const struct timeval timeout = {4, 0};
    return connections[index].server < FD_SETSIZE && flags >= 0 &&
        fcntl(connections[index].server, F_SETFL, flags | O_NONBLOCK) == 0 &&
        setsockopt(connections[index].peer, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == 0;
#endif
}
static void send_text(size_t index, const char *text)
{
    size_t remaining = strlen(text);
    while (remaining) {
        int sent = send(connections[index].peer, text, (int)remaining, 0);
        TEST_ASSERT_TRUE(sent > 0); text += sent; remaining -= (size_t)sent;
    }
}
static void submit(size_t index, const char *text, uint64_t accepted)
{
    TEST_ASSERT_TRUE(connect_pair(index)); send_text(index, text);
    TEST_ASSERT_TRUE(numforge_web_workers_submit(pool, connections[index].server, accepted));
    connections[index].server = NUMFORGE_INVALID_SOCKET;
}
static void expect_response(size_t index, const char *status, const char *body)
{
    char response[4096]; size_t used = 0U;
    while (used + 1U < sizeof(response)) {
        int received = recv(connections[index].peer, response + used, (int)(sizeof(response) - used - 1U), 0);
        TEST_ASSERT_TRUE(received >= 0);
        if (!received) break;
        used += (size_t)received;
    }
    response[used] = '\0';
    TEST_ASSERT_EQUAL_INT(0, strncmp(response, status, strlen(status)));
    TEST_ASSERT_NOT_NULL(strstr(response, body));
}
static bool await_counts(size_t pending, size_t active)
{
    uint64_t deadline = numforge_monotonic_ms() + 3000U;
    for (;;) {
        size_t observed = 0U;
        if (numforge_web_workers_pending(pool, &observed) == pending && observed == active) return true;
        if (numforge_monotonic_ms() >= deadline) return false;
        pause_thread();
    }
}
void setUp(void)
{
    for (size_t i = 0U; i < 5U; ++i) connections[i].server = connections[i].peer = NUMFORGE_INVALID_SOCKET;
#ifdef _WIN32
    WSADATA data; TEST_ASSERT_EQUAL_INT(0, WSAStartup(MAKEWORD(2, 2), &data));
#else
    TEST_ASSERT_TRUE(signal(SIGPIPE, SIG_IGN) != SIG_ERR);
#endif
}
void tearDown(void)
{
    numforge_web_workers_stop(pool); pool = NULL;
    for (size_t i = 0U; i < 5U; ++i) {
        if (connections[i].peer != NUMFORGE_INVALID_SOCKET) numforge_close_socket(connections[i].peer);
        if (connections[i].server != NUMFORGE_INVALID_SOCKET) numforge_close_socket(connections[i].server);
    }
    TEST_ASSERT_TRUE(numforge_web_server_shutdown());
#ifdef _WIN32
    WSACleanup();
#endif
}
static void test_configuration_and_empty_shutdown(void)
{
    TEST_ASSERT_NULL(numforge_web_workers_start(0, 1, 8765, NULL));
    TEST_ASSERT_NULL(numforge_web_workers_start(NUMFORGE_WEB_MAX_WORKERS + 1U, 1, 8765, NULL));
    TEST_ASSERT_NULL(numforge_web_workers_start(1, 0, 8765, NULL));
    TEST_ASSERT_NULL(numforge_web_workers_start(1, NUMFORGE_WEB_MAX_QUEUE + 1U, 8765, NULL));
    TEST_ASSERT_FALSE(numforge_web_workers_submit(NULL, NUMFORGE_INVALID_SOCKET, 0));
    size_t active = 1U; TEST_ASSERT_EQUAL_UINT(0, numforge_web_workers_pending(NULL, &active));
    TEST_ASSERT_EQUAL_UINT(0, active);
    pool = numforge_web_workers_start(2, 2, 8765, NULL); TEST_ASSERT_NOT_NULL(pool);
    TEST_ASSERT_FALSE(numforge_web_workers_submit(pool, NUMFORGE_INVALID_SOCKET, 0));
    TEST_ASSERT_TRUE(await_counts(0, 0));
}
static void test_slow_connection_does_not_block_another_worker(void)
{
    pool = numforge_web_workers_start(2, 2, 8765, NULL); TEST_ASSERT_NOT_NULL(pool);
    submit(0, "G", numforge_monotonic_ms()); TEST_ASSERT_TRUE(await_counts(0, 1));
    submit(1, calculate, numforge_monotonic_ms());
    expect_response(1, "HTTP/1.1 200 OK", "\"result\":\"4\"");
    TEST_ASSERT_TRUE(await_counts(0, 1));
    send_text(0, request_tail); expect_response(0, "HTTP/1.1 404 Not Found", "Not found");
    TEST_ASSERT_TRUE(await_counts(0, 0));
}
static void test_full_queue_rejects_without_stealing_socket_ownership_and_recovers(void)
{
    pool = numforge_web_workers_start(1, 2, 8765, NULL); TEST_ASSERT_NOT_NULL(pool);
    submit(0, "G", numforge_monotonic_ms()); TEST_ASSERT_TRUE(await_counts(0, 1));
    submit(1, calculate, numforge_monotonic_ms()); submit(2, calculate, numforge_monotonic_ms());
    TEST_ASSERT_TRUE(await_counts(2, 1));
    TEST_ASSERT_TRUE(connect_pair(3)); send_text(3, calculate);
    TEST_ASSERT_FALSE(numforge_web_workers_submit(pool, connections[3].server, numforge_monotonic_ms()));
    numforge_reject_connection(connections[3].server); numforge_close_socket(connections[3].server);
    connections[3].server = NUMFORGE_INVALID_SOCKET;
    expect_response(3, "HTTP/1.1 503 Service Unavailable", "\"code\":\"server_busy\"");
    TEST_ASSERT_TRUE(await_counts(2, 1));
    send_text(0, request_tail); expect_response(0, "HTTP/1.1 404 Not Found", "Not found");
    expect_response(1, "HTTP/1.1 200 OK", "\"result\":\"4\"");
    expect_response(2, "HTTP/1.1 200 OK", "\"result\":\"4\"");
    TEST_ASSERT_TRUE(await_counts(0, 0));
    submit(4, calculate, numforge_monotonic_ms()); expect_response(4, "HTTP/1.1 200 OK", "\"result\":\"4\"");
}
static void test_queue_expiry_does_not_need_a_free_worker(void)
{
    pool = numforge_web_workers_start(1, 2, 8765, NULL); TEST_ASSERT_NOT_NULL(pool);
    submit(0, "G", numforge_monotonic_ms()); TEST_ASSERT_TRUE(await_counts(0, 1));
    uint64_t accepted = numforge_monotonic_ms();
    submit(1, calculate, accepted); submit(2, calculate, accepted);
    numforge_web_workers_expire(pool, accepted + NUMFORGE_WEB_QUEUE_TIMEOUT_MS - 1U);
    TEST_ASSERT_TRUE(await_counts(2, 1));
    numforge_web_workers_expire(pool, accepted + NUMFORGE_WEB_QUEUE_TIMEOUT_MS);
    TEST_ASSERT_TRUE(await_counts(0, 1));
    expect_response(1, "HTTP/1.1 503 Service Unavailable", "server_busy");
    expect_response(2, "HTTP/1.1 503 Service Unavailable", "server_busy");
    send_text(0, request_tail); expect_response(0, "HTTP/1.1 404 Not Found", "Not found");
}
static void test_shutdown_discards_queue_and_joins_the_active_owner(void)
{
    pool = numforge_web_workers_start(1, 1, 8765, NULL); TEST_ASSERT_NOT_NULL(pool);
    submit(0, "G", numforge_monotonic_ms()); TEST_ASSERT_TRUE(await_counts(0, 1));
    submit(1, calculate, numforge_monotonic_ms()); TEST_ASSERT_TRUE(await_counts(1, 1));
    numforge_web_workers_stop(pool); pool = NULL;
    expect_response(0, "HTTP/1.1 408 Request Timeout", "request timeout");
    expect_response(1, "HTTP/1.1 503 Service Unavailable", "server_busy");
    pool = numforge_web_workers_start(1, 1, 8765, NULL); TEST_ASSERT_NOT_NULL(pool);
    submit(2, calculate, numforge_monotonic_ms()); expect_response(2, "HTTP/1.1 200 OK", "\"result\":\"4\"");
}
static void test_receive_deadline_is_not_restarted_after_queueing(void)
{
    pool = numforge_web_workers_start(1, 1, 8765, NULL); TEST_ASSERT_NOT_NULL(pool);
    submit(0, "G", numforge_monotonic_ms()); TEST_ASSERT_TRUE(await_counts(0, 1));
    /* One queued connection is already 800 ms old. It still fits the queue
     * deadline, but must receive only the remaining request-reading budget. */
    submit(1, "G", numforge_monotonic_ms() - 800U);
    uint64_t started = numforge_monotonic_ms();
    send_text(0, request_tail); expect_response(0, "HTTP/1.1 404 Not Found", "Not found");
    expect_response(1, "HTTP/1.1 408 Request Timeout", "request timeout");
    TEST_ASSERT_TRUE(numforge_monotonic_ms() - started < 1800U);
}
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_configuration_and_empty_shutdown);
    RUN_TEST(test_slow_connection_does_not_block_another_worker);
    RUN_TEST(test_full_queue_rejects_without_stealing_socket_ownership_and_recovers);
    RUN_TEST(test_queue_expiry_does_not_need_a_free_worker);
    RUN_TEST(test_shutdown_discards_queue_and_joins_the_active_owner);
    RUN_TEST(test_receive_deadline_is_not_restarted_after_queueing);
    return UNITY_END();
}
