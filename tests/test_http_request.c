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

static void test_public_origin_is_explicit_and_exact(void)
{
    NumForgeHttpFrame frame;
    const char *origin = "https://calculator.example.com";
    const char *allowed = "POST /api/evaluate HTTP/1.1\r\nContent-Length: 3\r\nOrigin: https://calculator.example.com\r\n\r\n2+2";
    const char *foreign = "POST /api/evaluate HTTP/1.1\r\nContent-Length: 3\r\nOrigin: https://calculator.example.com.evil.test\r\n\r\n2+2";
    TEST_ASSERT_EQUAL(NUMFORGE_HTTP_READY, numforge_http_probe(allowed, strlen(allowed), 8192, 8765, &frame));
    TEST_ASSERT_FALSE(frame.origin_allowed);
    TEST_ASSERT_EQUAL(NUMFORGE_HTTP_READY, numforge_http_probe_with_origin(allowed, strlen(allowed), 8192, 8765, origin, &frame));
    TEST_ASSERT_TRUE(frame.origin_allowed);
    TEST_ASSERT_EQUAL(NUMFORGE_HTTP_READY, numforge_http_probe_with_origin(foreign, strlen(foreign), 8192, 8765, origin, &frame));
    TEST_ASSERT_FALSE(frame.origin_allowed);
}

static void test_public_origin_configuration(void)
{
    TEST_ASSERT_TRUE(numforge_http_valid_origin("https://calculator.example.com"));
    TEST_ASSERT_TRUE(numforge_http_valid_origin("http://192.168.1.4:8765"));
    TEST_ASSERT_TRUE(numforge_http_valid_origin("https://calculator.example.com:8443"));
    const char *bad[] = { NULL, "", "*", "null", "https://", "ftp://example.com",
        "https://EXAMPLE.com", "https://user@example.com", "https://example.com/",
        "https://example.com?x", "https://example.com#x", "https://example.com\r\nX: y",
        "https://example.com:0", "https://example.com:65536", "https://example.com:443",
        "http://example.com:80", "https://example.com:0443" };
    for (size_t index = 0U; index < sizeof(bad) / sizeof(bad[0]); index++)
    {
        TEST_ASSERT_FALSE(numforge_http_valid_origin(bad[index]));
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_fragmentation_and_metadata);
    RUN_TEST(test_bad_and_oversized_headers);
    RUN_TEST(test_public_origin_is_explicit_and_exact);
    RUN_TEST(test_public_origin_configuration);
    return UNITY_END();
}
