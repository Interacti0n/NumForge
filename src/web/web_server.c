#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>

typedef SOCKET NumForgeSocket;
#define NUMFORGE_INVALID_SOCKET INVALID_SOCKET
#define numforge_close_socket closesocket
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>

typedef int NumForgeSocket;
#define NUMFORGE_INVALID_SOCKET (-1)
#define numforge_close_socket close
#endif

#include "web_api.h"
#include "http_request.h"
#include "web_page.h"
#include <numforge/runtime.h>

#define NUMFORGE_WEB_PORT 8765
#define NUMFORGE_WEB_SOCKET_TIMEOUT_MS 2000

/*
------------------------------------------------------------------------------------------------------------------------------
    Minimal loopback-only HTTP server for local NumForge demonstrations. It is
    intentionally not an Internet-facing server: it accepts one request at a
    time, serves one embedded page, and sends calculator expressions to the C
    parser and BigDecimal evaluator through web_api.c.
------------------------------------------------------------------------------------------------------------------------------
*/

/*
------------------------------------------------------------------------------------------------------------------------------
    HTTP response and request parsing helpers.
------------------------------------------------------------------------------------------------------------------------------
*/
static bool numforge_socket_retryable(void)
{
#ifdef _WIN32
    int error = WSAGetLastError();
    return error == WSAEWOULDBLOCK || error == WSAEINTR;
#else
    return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR;
#endif
}

static bool numforge_wait_socket(NumForgeSocket socket_value, bool writing, uint64_t deadline)
{
    for (;;)
    {
        fd_set ready;
        struct timeval timeout;
        uint64_t now = numforge_monotonic_ms();
        uint64_t remaining;
        int selected;
        if (now == UINT64_MAX || now >= deadline) return false;
        remaining = deadline - now;
        timeout.tv_sec = (long)(remaining / 1000U);
        timeout.tv_usec = (long)(remaining % 1000U) * 1000L;
        FD_ZERO(&ready);
        FD_SET(socket_value, &ready);
#ifdef _WIN32
        selected = select(0, writing ? NULL : &ready, writing ? &ready : NULL, NULL, &timeout);
        if (selected < 0 && WSAGetLastError() == WSAEINTR) continue;
#else
        selected = select(socket_value + 1, writing ? NULL : &ready, writing ? &ready : NULL, NULL, &timeout);
        if (selected < 0 && errno == EINTR) continue;
#endif
        return selected > 0;
    }
}

static bool numforge_send_all(NumForgeSocket socket, const char *data, size_t length, uint64_t deadline)
{
    while (length > 0U)
    {
        if (!numforge_wait_socket(socket, true, deadline)) return false;
        int sent = send(socket, data, (int)(length > 32767U ? 32767U : length), 0);
        if (sent < 0 && numforge_socket_retryable()) continue;

        if (sent <= 0)
        {
            return false;
        }
        data += (size_t)sent;
        length -= (size_t)sent;
    }

    return true;
}

static void numforge_send_response(
    NumForgeSocket socket,
    int status,
    const char *status_text,
    const char *content_type,
    const char *body
)
{
    char header[256];
    uint64_t deadline = numforge_monotonic_ms() + NUMFORGE_WEB_SOCKET_TIMEOUT_MS;
    int length = snprintf(header, sizeof(header),
                          "HTTP/1.1 %d %s\r\n"
                          "Content-Type: %s\r\n"
                          "Content-Length: %zu\r\n"
                          "Connection: close\r\n"
                          "Cache-Control: no-store\r\n\r\n",
                          status, status_text, content_type, strlen(body));

    if (length > 0 && (size_t)length < sizeof(header))
    {
        if (numforge_send_all(socket, header, (size_t)length, deadline))
            (void)numforge_send_all(socket, body, strlen(body), deadline);
    }
}

static void numforge_send_page(NumForgeSocket socket, const char *const *parts)
{
    char header[256];
    uint64_t deadline = numforge_monotonic_ms() + NUMFORGE_WEB_SOCKET_TIMEOUT_MS;
    size_t content_length = 0U;
    size_t index;
    int header_length;

    for (index = 0U; parts[index] != NULL; index++)
    {
        content_length += strlen(parts[index]);
    }

    header_length = snprintf(header, sizeof(header),
                             "HTTP/1.1 200 OK\r\n"
                             "Content-Type: text/html; charset=utf-8\r\n"
                             "Content-Length: %zu\r\n"
                             "Connection: close\r\n"
                             "Cache-Control: no-store\r\n\r\n",
                             content_length);
    if (header_length <= 0 || (size_t)header_length >= sizeof(header))
    {
        return;
    }

    if (!numforge_send_all(socket, header, (size_t)header_length, deadline)) return;
    for (index = 0U; parts[index] != NULL; index++)
    {
        if (!numforge_send_all(socket, parts[index], strlen(parts[index]), deadline)) return;
    }
}

static const char *numforge_find_header_end(const char *request)
{
    return strstr(request, "\r\n\r\n");
}

static bool numforge_read_request(
    NumForgeSocket socket, char *buffer, size_t capacity, size_t *length,
    bool *has_content_length, bool *origin_allowed, uint16_t port, int *http_error)
{
    size_t used = 0U;
    uint64_t deadline = numforge_monotonic_ms() + NUMFORGE_WEB_SOCKET_TIMEOUT_MS;
    *http_error = 400;
    for (;;)
    {
        NumForgeHttpFrame frame;
        NumForgeHttpStatus status = numforge_http_probe(buffer, used, capacity, port, &frame);
        if (status == NUMFORGE_HTTP_READY)
        {
            *length = frame.length;
            *has_content_length = frame.has_content_length;
            *origin_allowed = frame.origin_allowed;
            buffer[*length] = '\0';
            return true;
        }
        if (status != NUMFORGE_HTTP_MORE)
        {
            *http_error = status == NUMFORGE_HTTP_LARGE ? 413 : 400;
            return false;
        }
        if (!numforge_wait_socket(socket, false, deadline))
        {
            *http_error = 408;
            return false;
        }
        int received = recv(socket, buffer + used, (int)(capacity - used - 1U), 0);
        if (received < 0 && numforge_socket_retryable()) continue;
        if (received <= 0) return false;
        used += (size_t)received;
    }
}

static bool numforge_is_evaluation_target(const char *target)
{
    static const char prefix[] = "/api/evaluate?precision=";

    return strcmp(target, "/api/evaluate") == 0 ||
           strncmp(target, prefix, sizeof(prefix) - 1U) == 0;
}

static bool numforge_parse_output_scale(const char *target, int64_t *output_scale)
{
    const char *value;
    char *end;
    long long parsed;

    if (target == NULL || output_scale == NULL)
    {
        return false;
    }
    if (strcmp(target, "/api/evaluate") == 0)
    {
        *output_scale = CALCULATOR_DEFAULT_OUTPUT_SCALE;
        return true;
    }
    if (strncmp(target, "/api/evaluate?precision=", strlen("/api/evaluate?precision=")) != 0)
    {
        return false;
    }

    value = target + strlen("/api/evaluate?precision=");
    if (strcmp(value, "full") == 0)
    {
        *output_scale = CALCULATOR_UNLIMITED_OUTPUT_SCALE;
        return true;
    }

    errno = 0;
    parsed = strtoll(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0' || parsed < 0)
    {
        return false;
    }

    *output_scale = (int64_t)parsed;
    return true;
}

static bool numforge_parse_page_language(const char *target, const char *path, bool *english)
{
    size_t path_length;

    if (target == NULL || path == NULL || english == NULL)
    {
        return false;
    }

    path_length = strlen(path);
    if (strcmp(target, path) == 0 ||
        (strncmp(target, path, path_length) == 0 && strcmp(target + path_length, "?lang=sk") == 0))
    {
        *english = false;
        return true;
    }
    if (strncmp(target, path, path_length) == 0 && strcmp(target + path_length, "?lang=en") == 0)
    {
        *english = true;
        return true;
    }

    return false;
}

static void numforge_handle_evaluation(
    NumForgeSocket socket,
    const char *body,
    int64_t output_scale
)
{
    CalculatorError error;
    CalculatorStatus status;
    char *result = NULL;
    char *response;
    size_t response_capacity;

    status = numforge_web_evaluate_with_output_scale(body, output_scale, &result, &error);
    if (status == CALCULATOR_OK)
    {
        response_capacity = strlen(result) + 32U;
        response = malloc(response_capacity);
        if (response != NULL)
        {
            (void)snprintf(response, response_capacity, "{\"ok\":true,\"result\":\"%s\"}", result);
            numforge_send_response(socket, 200, "OK", "application/json; charset=utf-8", response);
            free(response);
        }
        else
        {
            numforge_send_response(socket, 500, "Internal Server Error", "application/json; charset=utf-8",
                                   "{\"ok\":false,\"error\":\"out of memory\",\"status\":\"out of memory\",\"column\":1}");
        }
        free(result);
        return;
    }

    {
        char error_response[256];
        const char *status_text = calculator_status_to_string(status);
        size_t column = calculator_error_column(body, error.offset);
        int response_length = snprintf(
            error_response,
            sizeof(error_response),
            "{\"ok\":false,\"error\":\"%s at column %zu\",\"status\":\"%s\",\"column\":%zu}",
            status_text,
            column,
            status_text,
            column);

        if (response_length > 0 && (size_t)response_length < sizeof(error_response))
        {
            int http_status = status == CALCULATOR_OUT_OF_MEMORY ? 500 : 400;
            const char *http_status_text = http_status == 500 ? "Internal Server Error" : "Bad Request";

            numforge_send_response(socket, http_status, http_status_text,
                                   "application/json; charset=utf-8", error_response);
        }
        else
        {
            numforge_send_response(socket, 500, "Internal Server Error",
                                   "application/json; charset=utf-8",
                                   "{\"ok\":false,\"error\":\"failed to format error response\",\"status\":\"out of memory\",\"column\":1}");
        }
    }
}

static void numforge_handle_connection(NumForgeSocket socket, uint16_t port)
{
    char request[NUMFORGE_WEB_REQUEST_CAPACITY];
    char method[16];
    char target[128];
    const char *body;
    size_t length;
    size_t body_length = 0U;
    int64_t output_scale;
    bool english;
    bool has_content_length;
    bool origin_allowed;
    int http_error = 400;

    if (!numforge_read_request(
            socket, request, sizeof(request), &length, &has_content_length,
            &origin_allowed, port, &http_error) ||
        !numforge_request_target(request, method, sizeof(method), target, sizeof(target)))
    {
        const char *reason = http_error == 413 ? "Payload Too Large" :
            (http_error == 408 ? "Request Timeout" : "Bad Request");
        const char *body_text = http_error == 413 ?
            "{\"ok\":false,\"error\":\"request exceeds 4096 bytes\",\"status\":\"value too large\",\"column\":1}" :
            (http_error == 408 ? "{\"ok\":false,\"error\":\"request timeout\"}" :
             "{\"ok\":false,\"error\":\"Bad request\",\"status\":\"invalid argument\",\"column\":1}");
        numforge_send_response(socket, http_error, reason, "application/json; charset=utf-8", body_text);
        if (http_error == 413)
        {
            /* Drain a bounded remainder so normal oversized requests can read
             * the response instead of receiving a reset on close. */
            char discarded[1024];
            size_t drained = 0U;
            uint64_t deadline = numforge_monotonic_ms() + 200U;
            while (drained < NUMFORGE_WEB_REQUEST_CAPACITY && numforge_wait_socket(socket, false, deadline))
            {
                int received = recv(socket, discarded, sizeof(discarded), 0);
                if (received <= 0) break;
                drained += (size_t)received;
            }
        }
        return;
    }

    body = numforge_find_header_end(request);
    if (body != NULL)
    {
        body += 4;
        body_length = length - (size_t)(body - request);
    }

    if (strcmp(method, "GET") == 0 && numforge_parse_page_language(target, "/", &english))
    {
        numforge_send_page(socket, english ? NUMFORGE_WEB_PAGE_EN : NUMFORGE_WEB_PAGE);
    }
    else if (strcmp(method, "GET") == 0 && numforge_parse_page_language(target, "/api", &english))
    {
        numforge_send_page(socket, english ? NUMFORGE_API_PAGE_EN : NUMFORGE_API_PAGE);
    }
    else if (strcmp(method, "POST") == 0 && !origin_allowed)
    {
        numforge_send_response(
            socket, 403, "Forbidden", "application/json; charset=utf-8",
            "{\"ok\":false,\"error\":\"forbidden origin\",\"status\":\"invalid argument\",\"column\":1}");
    }
    else if (strcmp(method, "POST") == 0 && !has_content_length)
    {
        numforge_send_response(socket, 411, "Length Required", "text/plain; charset=utf-8",
                               "Content-Length is required.\n");
    }
    else if (strcmp(method, "POST") == 0 && body != NULL &&
             numforge_is_evaluation_target(target))
    {
        if (memchr(body, '\0', body_length) != NULL)
        {
            numforge_send_response(
                socket, 400, "Bad Request", "application/json; charset=utf-8",
                "{\"ok\":false,\"error\":\"request body contains a NUL byte\",\"status\":\"invalid argument\",\"column\":1}");
        }
        else if (numforge_parse_output_scale(target, &output_scale))
        {
            numforge_handle_evaluation(socket, body, output_scale);
        }
        else
        {
            numforge_send_response(
                socket, 400, "Bad Request", "application/json; charset=utf-8",
                "{\"ok\":false,\"error\":\"invalid precision\",\"status\":\"invalid argument\",\"column\":1}");
        }
    }
    else
    {
        numforge_send_response(socket, 404, "Not Found", "text/plain; charset=utf-8", "Not found.\n");
    }
}

/*
------------------------------------------------------------------------------------------------------------------------------
    Platform networking and browser-launch helpers.
------------------------------------------------------------------------------------------------------------------------------
*/
static bool numforge_networking_start(void)
{
#ifdef _WIN32
    WSADATA data;

    return WSAStartup(MAKEWORD(2, 2), &data) == 0;
#else
    return signal(SIGPIPE, SIG_IGN) != SIG_ERR;
#endif
}

static void numforge_networking_stop(void)
{
#ifdef _WIN32
    WSACleanup();
#endif
}

static void numforge_open_browser(uint16_t port, bool enabled)
{
#ifdef _WIN32
    const char *disabled = getenv("NUMFORGE_WEB_NO_BROWSER");
    char url[64];

    if (enabled && (disabled == NULL || strcmp(disabled, "1") != 0) &&
        snprintf(url, sizeof(url), "http://127.0.0.1:%u", (unsigned int)port) > 0)
    {
        (void)ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
    }
#else
    (void)port;
    (void)enabled;
#endif
}

static bool numforge_configure_client_socket(NumForgeSocket socket)
{
#ifdef _WIN32
    u_long nonblocking = 1UL;
    return ioctlsocket(socket, FIONBIO, &nonblocking) == 0;
#else
    int flags;
    if (socket >= FD_SETSIZE) return false;
    flags = fcntl(socket, F_GETFL, 0);
    return flags >= 0 && fcntl(socket, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
}

/*
------------------------------------------------------------------------------------------------------------------------------
    Local web server entry point.
------------------------------------------------------------------------------------------------------------------------------
*/
static bool numforge_parse_port(const char *text, uint16_t *port)
{
    char *end;
    unsigned long parsed;

    if (text == NULL || port == NULL || *text == '\0')
    {
        return false;
    }

    errno = 0;
    end = NULL;
    parsed = strtoul(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || parsed == 0UL || parsed > 65535UL)
    {
        return false;
    }

    *port = (uint16_t)parsed;
    return true;
}

static bool numforge_parse_options(
    int argc,
    char **argv,
    uint16_t *port,
    bool *open_browser
)
{
    int index;

    if (port == NULL || open_browser == NULL)
    {
        return false;
    }

    *port = NUMFORGE_WEB_PORT;
    *open_browser = true;

    for (index = 1; index < argc; index++)
    {
        if (strcmp(argv[index], "--no-browser") == 0)
        {
            *open_browser = false;
        }
        else if (strcmp(argv[index], "--port") == 0 && index + 1 < argc)
        {
            index++;
            if (!numforge_parse_port(argv[index], port))
            {
                return false;
            }
        }
        else
        {
            return false;
        }
    }

    return true;
}

int main(int argc, char **argv)
{
    NumForgeSocket listener;
    struct sockaddr_in address;
    int reuse_address = 1;
    uint16_t port;
    bool open_browser;

    if (!numforge_parse_options(argc, argv, &port, &open_browser))
    {
        fputs("Usage: numforge_web [--port 1-65535] [--no-browser]\n", stderr);
        return 2;
    }

    if (!numforge_networking_start())
    {
        fputs("NumForge web: failed to initialize networking\n", stderr);
        return 1;
    }

    listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == NUMFORGE_INVALID_SOCKET)
    {
        fputs("NumForge web: failed to create server socket\n", stderr);
        numforge_networking_stop();
        return 1;
    }

#ifdef _WIN32
    /* Winsock SO_REUSEADDR permits another process to bind this same port.
     * Exclusive ownership keeps a second server from stealing requests. */
    if (setsockopt(listener, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                   (const char *)&reuse_address, sizeof(reuse_address)) != 0)
    {
        fputs("NumForge web: failed to reserve exclusive socket ownership\n", stderr);
        numforge_close_socket(listener);
        numforge_networking_stop();
        return 1;
    }
#else
    (void)setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse_address, sizeof(reuse_address));
#endif
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(port);

    if (bind(listener, (const struct sockaddr *)&address, sizeof(address)) != 0 || listen(listener, 8) != 0)
    {
        fprintf(stderr, "NumForge web: port %u is unavailable\n", (unsigned int)port);
        numforge_close_socket(listener);
        numforge_networking_stop();
        return 1;
    }

    printf("NumForge web is running at http://127.0.0.1:%u\n", (unsigned int)port);
    puts("Press Ctrl+C to stop the local server.");
    numforge_open_browser(port, open_browser);

    for (;;)
    {
        NumForgeSocket client = accept(listener, NULL, NULL);

        if (client != NUMFORGE_INVALID_SOCKET)
        {
            if (numforge_configure_client_socket(client)) numforge_handle_connection(client, port);
            numforge_close_socket(client);
        }
    }
}
