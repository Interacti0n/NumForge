#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#include "apps/web/server/web_workers.h"
#include "apps/web/server/web_server.h"
#include <numforge/runtime.h>
#include <unity.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <process.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>
#include <pthread.h>
#include <stdatomic.h>
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#endif

static NumForgeWebWorkers *pool;
#ifdef _WIN32
typedef volatile LONG TestFlag;
static bool flag_get(TestFlag *flag) { return InterlockedCompareExchange(flag, 0, 0) != 0; }
static void flag_set(TestFlag *flag, bool value) { (void)InterlockedExchange(flag, value ? 1 : 0); }
static HANDLE stop_thread;
#else
typedef atomic_bool TestFlag;
static bool flag_get(TestFlag *flag) { return atomic_load(flag); }
static void flag_set(TestFlag *flag, bool value) { atomic_store(flag, value); }
static pthread_t stop_thread;
#endif
static TestFlag release_compute, stop_finished;
static bool stop_started;
static struct { NumForgeSocket server, peer; } connections[NUMFORGE_WEB_MAX_PENDING + 2U];
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
/* Scheduling barriers avoid hardware-dependent expensive calculations in unit
 * tests. Actual arithmetic under load belongs to the separate HTTP benchmark. */
static void controlled_handler(NumForgeSocket socket, char *request, const NumForgeHttpFrame *frame)
{
    if (!strcmp(frame->target, "/blocked"))
        while (!flag_get(&release_compute)) pause_thread();
    numforge_handle_framed_connection(socket, request, frame);
}
#ifdef _WIN32
static unsigned int __stdcall stop_pool(void *argument)
#else
static void *stop_pool(void *argument)
#endif
{
    numforge_web_workers_stop(argument);
    flag_set(&stop_finished, true);
    return 0;
}
static void join_stopper(void)
{
    if (!stop_started) return;
#ifdef _WIN32
    TEST_ASSERT_EQUAL(WAIT_OBJECT_0, WaitForSingleObject(stop_thread, 5000U));
    TEST_ASSERT_TRUE(CloseHandle(stop_thread));
#else
    TEST_ASSERT_EQUAL_INT(0, pthread_join(stop_thread, NULL));
#endif
    stop_started = false;
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
    char response[4096]; size_t used = 0U, framed = 0U;
    while (used + 1U < sizeof(response)) {
        int received = recv(connections[index].peer, response + used, (int)(sizeof(response) - used - 1U), 0);
        TEST_ASSERT_TRUE(received >= 0);
        if (!received) break;
        used += (size_t)received;
        response[used] = '\0';
        /* A rejected connection can retain unread request bytes. Validate its
         * complete HTTP frame without requiring a clean TCP EOF afterwards. */
        if (!framed) {
            const char *headers = strstr(response, "\r\n\r\n");
            if (headers) {
                const char *length = strstr(response, "\r\nContent-Length: ");
                TEST_ASSERT_TRUE(length != NULL && length < headers);
                const char *digits = length + strlen("\r\nContent-Length: ");
                char *end = NULL;
                unsigned long body_length = strtoul(digits, &end, 10);
                TEST_ASSERT_TRUE(*digits >= '0' && *digits <= '9' && end <= headers && end[0] == '\r' && end[1] == '\n');
                size_t header_length = (size_t)(headers - response) + 4U;
                TEST_ASSERT_TRUE(body_length <= sizeof(response) - header_length - 1U);
                framed = header_length + (size_t)body_length;
            }
        }
        if (framed && used >= framed) break;
    }
    response[used] = '\0';
    TEST_ASSERT_TRUE(framed != 0U && used == framed);
    TEST_ASSERT_EQUAL_INT(0, strncmp(response, status, strlen(status)));
    TEST_ASSERT_NOT_NULL(strstr(response, body));
}
static bool await_counts(size_t pending, size_t active)
{
    uint64_t deadline = numforge_monotonic_ms() + 3000U;
    for (;;) {
        size_t observed = 0U;
        if (numforge_web_workers_pending(pool, &observed, NULL) == pending && observed == active) return true;
        if (numforge_monotonic_ms() >= deadline) return false;
        pause_thread();
    }
}
void setUp(void)
{
    flag_set(&release_compute, false); flag_set(&stop_finished, false);
    for (size_t i = 0U; i < sizeof(connections)/sizeof(connections[0]); ++i) connections[i].server = connections[i].peer = NUMFORGE_INVALID_SOCKET;
#ifdef _WIN32
    WSADATA data; TEST_ASSERT_EQUAL_INT(0, WSAStartup(MAKEWORD(2, 2), &data));
#else
    TEST_ASSERT_TRUE(signal(SIGPIPE, SIG_IGN) != SIG_ERR);
#endif
}
void tearDown(void)
{
    flag_set(&release_compute, true);
    join_stopper();
    numforge_web_workers_stop(pool); pool = NULL;
    for (size_t i = 0U; i < sizeof(connections)/sizeof(connections[0]); ++i) {
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
    TEST_ASSERT_NULL(numforge_web_workers_start_handler(1, 1, 8765, NULL, NULL));
    TEST_ASSERT_FALSE(numforge_web_workers_submit(NULL, NUMFORGE_INVALID_SOCKET, 0));
    size_t active = 1U; TEST_ASSERT_EQUAL_UINT(0, numforge_web_workers_pending(NULL, &active, NULL));
    TEST_ASSERT_EQUAL_UINT(0, active);
    pool = numforge_web_workers_start(2, 2, 8765, NULL); TEST_ASSERT_NOT_NULL(pool);
    TEST_ASSERT_FALSE(numforge_web_workers_submit(pool, NUMFORGE_INVALID_SOCKET, 0));
    TEST_ASSERT_TRUE(await_counts(0, 0));
}
static bool await_state(size_t pending, size_t active, size_t reading)
{
    uint64_t deadline = numforge_monotonic_ms() + 3000U;
    for (;;) {
        size_t a = 0U, r = 0U;
        if (numforge_web_workers_pending(pool, &a, &r) == pending && a == active && r == reading) return true;
        if (numforge_monotonic_ms() >= deadline) return false;
        pause_thread();
    }
}
static const char blocked[] = "GET /blocked HTTP/1.1\r\nHost: localhost\r\n\r\n";
static void finish_compute(size_t index)
{
    flag_set(&release_compute, true);
    expect_response(index, "HTTP/1.1 404 Not Found", "Not found");
}
static void test_partial_request_does_not_occupy_the_only_compute_worker(void)
{
    pool = numforge_web_workers_start(1, 2, 8765, NULL); TEST_ASSERT_NOT_NULL(pool);
    submit(0, "G", numforge_monotonic_ms()); TEST_ASSERT_TRUE(await_state(1, 0, 1));
    submit(1, calculate, numforge_monotonic_ms());
    expect_response(1, "HTTP/1.1 200 OK", "\"result\":\"4\"");
    TEST_ASSERT_TRUE(await_state(1, 0, 1));
    send_text(0, request_tail); expect_response(0, "HTTP/1.1 404 Not Found", "Not found");
    TEST_ASSERT_TRUE(await_state(0, 0, 0));
}
static void test_full_ingress_preserves_rejected_socket_ownership_and_recovers(void)
{
    pool = numforge_web_workers_start(1, 2, 8765, NULL); TEST_ASSERT_NOT_NULL(pool);
    submit(0, "G", numforge_monotonic_ms()); submit(1, "G", numforge_monotonic_ms());
    submit(2, "G", numforge_monotonic_ms());
    TEST_ASSERT_TRUE(await_state(3, 0, 3));
    TEST_ASSERT_TRUE(connect_pair(3)); send_text(3, calculate);
    TEST_ASSERT_FALSE(numforge_web_workers_submit(pool, connections[3].server, numforge_monotonic_ms()));
    numforge_reject_connection(connections[3].server); numforge_close_socket(connections[3].server);
    connections[3].server = NUMFORGE_INVALID_SOCKET;
    expect_response(3, "HTTP/1.1 503 Service Unavailable", "\"code\":\"server_busy\"");
    TEST_ASSERT_TRUE(await_state(3, 0, 3));
    send_text(0, request_tail); send_text(1, request_tail); send_text(2, request_tail);
    expect_response(0, "HTTP/1.1 404 Not Found", "Not found");
    expect_response(1, "HTTP/1.1 404 Not Found", "Not found");
    expect_response(2, "HTTP/1.1 404 Not Found", "Not found");
    TEST_ASSERT_TRUE(await_state(0, 0, 0));
    submit(4, calculate, numforge_monotonic_ms()); expect_response(4, "HTTP/1.1 200 OK", "\"result\":\"4\"");
}
static void test_ready_queue_expiry_does_not_need_a_free_compute_worker(void)
{
    pool = numforge_web_workers_start_handler(1, 2, 8765, NULL, controlled_handler); TEST_ASSERT_NOT_NULL(pool);
    submit(0, blocked, numforge_monotonic_ms()); TEST_ASSERT_TRUE(await_state(0, 1, 0));
    submit(1, calculate, numforge_monotonic_ms()); submit(2, calculate, numforge_monotonic_ms());
    TEST_ASSERT_TRUE(await_state(2, 1, 0));
    uint64_t now = numforge_monotonic_ms();
    numforge_web_workers_expire(pool, now);
    TEST_ASSERT_TRUE(await_state(2, 1, 0));
    numforge_web_workers_expire(pool, now + NUMFORGE_WEB_QUEUE_TIMEOUT_MS);
    TEST_ASSERT_TRUE(await_state(0, 1, 0));
    expect_response(1, "HTTP/1.1 503 Service Unavailable", "server_busy");
    expect_response(2, "HTTP/1.1 503 Service Unavailable", "server_busy");
    finish_compute(0);
}
static void test_original_deadline_can_expire_behind_a_younger_queue_head(void)
{
    pool = numforge_web_workers_start_handler(1, 3, 8765, NULL, controlled_handler); TEST_ASSERT_NOT_NULL(pool);
    submit(0, blocked, numforge_monotonic_ms()); TEST_ASSERT_TRUE(await_state(0, 1, 0));
    submit(1, "G", numforge_monotonic_ms() - 1500U);
    submit(2, calculate, numforge_monotonic_ms());
    TEST_ASSERT_TRUE(await_state(2, 1, 1));
    send_text(1, request_tail); TEST_ASSERT_TRUE(await_state(2, 1, 0));
    numforge_web_workers_expire(pool, numforge_monotonic_ms() + 600U);
    TEST_ASSERT_TRUE(await_state(1, 1, 0));
    expect_response(1, "HTTP/1.1 503 Service Unavailable", "server_busy");
    numforge_web_workers_expire(pool, UINT64_MAX);
    expect_response(2, "HTTP/1.1 503 Service Unavailable", "server_busy");
    finish_compute(0);
}
static void test_shutdown_discards_partial_and_ready_jobs_and_joins_compute(void)
{
    pool = numforge_web_workers_start_handler(1, 2, 8765, NULL, controlled_handler); TEST_ASSERT_NOT_NULL(pool);
    submit(0, blocked, numforge_monotonic_ms()); TEST_ASSERT_TRUE(await_state(0, 1, 0));
    submit(1, "G", numforge_monotonic_ms()); submit(2, calculate, numforge_monotonic_ms());
    TEST_ASSERT_TRUE(await_state(2, 1, 1));
#ifdef _WIN32
    stop_thread = (HANDLE)_beginthreadex(NULL, 0, stop_pool, pool, 0, NULL);
    TEST_ASSERT_NOT_NULL(stop_thread);
#else
    TEST_ASSERT_EQUAL_INT(0, pthread_create(&stop_thread, NULL, stop_pool, pool));
#endif
    stop_started = true; pool = NULL;
    expect_response(1, "HTTP/1.1 503 Service Unavailable", "server_busy");
    expect_response(2, "HTTP/1.1 503 Service Unavailable", "server_busy");
    TEST_ASSERT_FALSE(flag_get(&stop_finished));
    finish_compute(0); join_stopper(); TEST_ASSERT_TRUE(flag_get(&stop_finished));
    pool = numforge_web_workers_start(1, 1, 8765, NULL); TEST_ASSERT_NOT_NULL(pool);
    submit(3, calculate, numforge_monotonic_ms()); expect_response(3, "HTTP/1.1 200 OK", "\"result\":\"4\"");
}
static void test_receive_deadline_is_not_restarted_by_partial_bytes(void)
{
    pool = numforge_web_workers_start(1, 1, 8765, NULL); TEST_ASSERT_NOT_NULL(pool);
    uint64_t started = numforge_monotonic_ms();
    submit(0, "G", started - 800U); TEST_ASSERT_TRUE(await_state(1, 0, 1));
    send_text(0, "ET /missing HTTP/1.1\r\n");
    expect_response(0, "HTTP/1.1 408 Request Timeout", "request timeout");
    TEST_ASSERT_TRUE(numforge_monotonic_ms() - started < 1800U);
}
static void test_fragmented_body_and_peer_eof_release_their_slots(void)
{
    pool = numforge_web_workers_start(1, 2, 8765, NULL); TEST_ASSERT_NOT_NULL(pool);
    submit(0, "POST /api/evaluate HTTP/1.1\r\nHost: localhost\r\nContent-Length: 3\r\n\r\n2+", numforge_monotonic_ms());
    TEST_ASSERT_TRUE(await_state(1, 0, 1));
    submit(1, calculate, numforge_monotonic_ms()); expect_response(1, "HTTP/1.1 200 OK", "\"result\":\"4\"");
    send_text(0, "2"); expect_response(0, "HTTP/1.1 200 OK", "\"result\":\"4\"");
    TEST_ASSERT_TRUE(await_state(0, 0, 0));
    submit(2, "G", numforge_monotonic_ms()); TEST_ASSERT_TRUE(await_state(1, 0, 1));
#ifdef _WIN32
    TEST_ASSERT_EQUAL_INT(0, shutdown(connections[2].peer, SD_SEND));
#else
    TEST_ASSERT_EQUAL_INT(0, shutdown(connections[2].peer, SHUT_WR));
#endif
    expect_response(2, "HTTP/1.1 400 Bad Request", "Bad request");
    TEST_ASSERT_TRUE(await_state(0, 0, 0));
}
static void test_maximum_ingress_and_select_capacity(void)
{
    pool = numforge_web_workers_start(NUMFORGE_WEB_MAX_WORKERS, NUMFORGE_WEB_MAX_QUEUE, 8765, NULL); TEST_ASSERT_NOT_NULL(pool);
    for (size_t i = 0U; i < NUMFORGE_WEB_MAX_PENDING; ++i) submit(i, "G", numforge_monotonic_ms());
    TEST_ASSERT_TRUE(await_state(NUMFORGE_WEB_MAX_PENDING, 0, NUMFORGE_WEB_MAX_PENDING));
    /* Last slot is beyond Winsock's first fd_set batch. */
    send_text(NUMFORGE_WEB_MAX_PENDING - 1U, request_tail);
    expect_response(NUMFORGE_WEB_MAX_PENDING - 1U, "HTTP/1.1 404 Not Found", "Not found");
    TEST_ASSERT_TRUE(await_state(NUMFORGE_WEB_MAX_PENDING - 1U, 0, NUMFORGE_WEB_MAX_PENDING - 1U));
    submit(NUMFORGE_WEB_MAX_PENDING, calculate, numforge_monotonic_ms());
    expect_response(NUMFORGE_WEB_MAX_PENDING, "HTTP/1.1 200 OK", "\"result\":\"4\"");
    TEST_ASSERT_TRUE(await_state(NUMFORGE_WEB_MAX_PENDING - 1U, 0, NUMFORGE_WEB_MAX_PENDING - 1U));
}
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_configuration_and_empty_shutdown);
    RUN_TEST(test_partial_request_does_not_occupy_the_only_compute_worker);
    RUN_TEST(test_full_ingress_preserves_rejected_socket_ownership_and_recovers);
    RUN_TEST(test_ready_queue_expiry_does_not_need_a_free_compute_worker);
    RUN_TEST(test_original_deadline_can_expire_behind_a_younger_queue_head);
    RUN_TEST(test_shutdown_discards_partial_and_ready_jobs_and_joins_compute);
    RUN_TEST(test_receive_deadline_is_not_restarted_by_partial_bytes);
    RUN_TEST(test_fragmented_body_and_peer_eof_release_their_slots);
    RUN_TEST(test_maximum_ingress_and_select_capacity);
    return UNITY_END();
}
