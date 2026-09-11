#include <string.h>
#include <unity.h>
#include "../src/web/http_request.h"

void setUp(void) {}
void tearDown(void) {}

static void test_fragmentation_and_metadata(void)
{
    const char *request = "POST /api/evaluate HTTP/1.1\r\nContent-Length: 3\r\nOrigin: http://localhost:8765\r\n\r\n2+2";
    NumForgeHttpFrame frame;
    for (size_t size = 0; size < strlen(request); size++)
        TEST_ASSERT_EQUAL(NUMFORGE_HTTP_MORE, numforge_http_probe(request, size, 8192, 8765, &frame));
    TEST_ASSERT_EQUAL(NUMFORGE_HTTP_READY, numforge_http_probe(request, strlen(request), 8192, 8765, &frame));
    TEST_ASSERT_EQUAL_STRING("POST", frame.method);
    TEST_ASSERT_EQUAL_STRING("/api/evaluate", frame.target);
    TEST_ASSERT_TRUE(frame.origin_allowed);
    TEST_ASSERT_TRUE(frame.has_content_length);
    TEST_ASSERT_EQUAL_UINT(3, frame.length - frame.header_length);
    TEST_ASSERT_EQUAL(NUMFORGE_HTTP_READY, numforge_http_probe(request, strlen(request), 8192, 80, &frame));
    TEST_ASSERT_FALSE(frame.origin_allowed);
}

static void test_bad_and_oversized_headers(void)
{
    static const char *const bad[] = {
        "GET / HTTP/2\r\n\r\n",
        "POST / HTTP/1.1\r\nContent-Length: 1\r\nContent-Length: 1\r\n\r\nx",
        "POST / HTTP/1.1\r\nContent-Length: -1\r\n\r\n",
        "POST / HTTP/1.1\r\nContent-Length: 18446744073709551616\r\n\r\n",
        "POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n",
        "GET / HTTP/1.1\r\nOrigin: x\r\nOrigin: y\r\n\r\n",
        "GET / HTTP/1.1\r\nBroken header\r\n\r\n"
    };
    NumForgeHttpFrame frame;
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
        TEST_ASSERT_EQUAL(NUMFORGE_HTTP_BAD, numforge_http_probe(bad[i], strlen(bad[i]), 8192, 8765, &frame));
    const char *large = "POST / HTTP/1.1\r\nContent-Length: 4097\r\n\r\n";
    TEST_ASSERT_EQUAL(NUMFORGE_HTTP_LARGE, numforge_http_probe(large, strlen(large), 8192, 8765, &frame));
    const char nul[] = "GET / HTTP/1.1\r\nX:\0hidden\r\n\r\n";
    TEST_ASSERT_EQUAL(NUMFORGE_HTTP_BAD, numforge_http_probe(nul, sizeof(nul) - 1U, 8192, 8765, &frame));
    TEST_ASSERT_EQUAL(NUMFORGE_HTTP_BAD, numforge_http_probe("", 0, 0, 8765, &frame));
    TEST_ASSERT_EQUAL(NUMFORGE_HTTP_BAD, numforge_http_probe(NULL, 0, 8192, 8765, &frame));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_fragmentation_and_metadata);
    RUN_TEST(test_bad_and_oversized_headers);
    return UNITY_END();
}
