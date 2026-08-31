#include <errno.h>
#include <limits.h>
#include <stdbool.h>
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
#include <signal.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

typedef int NumForgeSocket;
#define NUMFORGE_INVALID_SOCKET (-1)
#define numforge_close_socket close
#endif

#include "web_api.h"
#include "web_page.h"

#define NUMFORGE_WEB_PORT 8765
#define NUMFORGE_WEB_REQUEST_CAPACITY 8192U
#define NUMFORGE_WEB_SOCKET_TIMEOUT_MS 5000

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
static bool numforge_send_all(NumForgeSocket socket, const char *data, size_t length)
{
    while (length > 0U)
    {
        int sent = send(socket, data, (int)(length > 32767U ? 32767U : length), 0);

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
    int length = snprintf(header, sizeof(header),
                          "HTTP/1.1 %d %s\r\n"
                          "Content-Type: %s\r\n"
                          "Content-Length: %zu\r\n"
                          "Connection: close\r\n"
                          "Cache-Control: no-store\r\n\r\n",
                          status, status_text, content_type, strlen(body));

    if (length > 0 && (size_t)length < sizeof(header))
    {
        (void)numforge_send_all(socket, header, (size_t)length);
        (void)numforge_send_all(socket, body, strlen(body));
    }
}

static void numforge_send_page(NumForgeSocket socket, const char *const *parts)
{
    char header[256];
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

    (void)numforge_send_all(socket, header, (size_t)header_length);
    for (index = 0U; parts[index] != NULL; index++)
    {
        (void)numforge_send_all(socket, parts[index], strlen(parts[index]));
    }
}

static const char *numforge_find_header_end(const char *request)
{
    return strstr(request, "\r\n\r\n");
}

static char numforge_ascii_lower(char character)
{
    return character >= 'A' && character <= 'Z'
        ? (char)(character + ('a' - 'A'))
        : character;
}

static bool numforge_ascii_equals(
    const char *name,
    size_t length,
    const char *expected
)
{
    size_t index;

    if (strlen(expected) != length)
    {
        return false;
    }
    for (index = 0U; index < length; index++)
    {
        if (numforge_ascii_lower(name[index]) != numforge_ascii_lower(expected[index]))
        {
            return false;
        }
    }
    return true;
}

static bool numforge_parse_request_headers(
    const char *request,
    const char *header_end,
    unsigned long long *body_length,
    bool *content_length_present,
    bool *origin_allowed
)
{
    const char *line = strstr(request, "\r\n");
    bool origin_present = false;

    *body_length = 0U;
    *content_length_present = false;
    *origin_allowed = true;
    if (line == NULL || line > header_end)
    {
        return false;
    }
    if (line == header_end)
    {
        return true;
    }
    line += 2;

    while (line < header_end)
    {
        const char *line_end = strstr(line, "\r\n");
        const char *colon;

        if (line_end == NULL || line_end > header_end)
        {
            return false;
        }
        colon = memchr(line, ':', (size_t)(line_end - line));
        if (colon == NULL)
        {
            return false;
        }

        if (numforge_ascii_equals(
                line, (size_t)(colon - line), "Content-Length"))
        {
            const char *value = colon + 1;
            unsigned long long parsed = 0U;

            if (*content_length_present)
            {
                return false;
            }
            while (value < line_end && (*value == ' ' || *value == '\t')) value++;
            if (value == line_end || *value < '0' || *value > '9')
            {
                return false;
            }
            while (value < line_end && *value >= '0' && *value <= '9')
            {
                unsigned int digit = (unsigned int)(*value - '0');

                if (parsed > (ULLONG_MAX - digit) / 10U)
                {
                    return false;
                }
                parsed = parsed * 10U + digit;
                value++;
            }
            while (value < line_end && (*value == ' ' || *value == '\t')) value++;
            if (value != line_end)
            {
                return false;
            }

            *body_length = parsed;
            *content_length_present = true;
        }
        else if (numforge_ascii_equals(
                     line, (size_t)(colon - line), "Origin"))
        {
            const char *value = colon + 1;
            const char *value_end = line_end;
            size_t value_length;

            if (origin_present)
            {
                return false;
            }
            origin_present = true;
            while (value < line_end && (*value == ' ' || *value == '\t')) value++;
            while (value_end > value &&
                   (value_end[-1] == ' ' || value_end[-1] == '\t')) value_end--;
            value_length = (size_t)(value_end - value);
            *origin_allowed =
                numforge_ascii_equals(value, value_length, "http://127.0.0.1:8765") ||
                numforge_ascii_equals(value, value_length, "http://localhost:8765");
        }
        if (line_end == header_end)
        {
            return true;
        }
        line = line_end + 2;
    }

    return false;
}

static bool numforge_read_request(
    NumForgeSocket socket,
    char *buffer,
    size_t capacity,
    size_t *length,
    bool *has_content_length,
    bool *origin_allowed
)
{
    size_t used = 0U;
    const char *header_end = NULL;
    size_t header_length;
    unsigned long long body_length = 0U;

    while (used + 1U < capacity)
    {
        int received = recv(socket, buffer + used, (int)(capacity - used - 1U), 0);

        if (received <= 0)
        {
            return false;
        }
        used += (size_t)received;
        buffer[used] = '\0';
        header_end = numforge_find_header_end(buffer);
        if (header_end != NULL)
        {
            break;
        }
    }

    if (header_end == NULL)
    {
        return false;
    }

    header_length = (size_t)(header_end - buffer) + 4U;
    if (!numforge_parse_request_headers(
            buffer, header_end, &body_length, has_content_length, origin_allowed))
    {
        return false;
    }
    if (body_length > capacity - header_length - 1U)
    {
        return false;
    }

    while (used < header_length + (size_t)body_length)
    {
        int received;

        if (used + 1U >= capacity)
        {
            return false;
        }
        received = recv(socket, buffer + used, (int)(capacity - used - 1U), 0);
        if (received <= 0)
        {
            return false;
        }
        used += (size_t)received;
        buffer[used] = '\0';
    }

    used = header_length + (size_t)body_length;
    buffer[used] = '\0';
    *length = used;
    return true;
}

static bool numforge_request_target(const char *request, char *method, size_t method_capacity,
                                    char *target, size_t target_capacity)
{
    const char *line_end;
    const char *first_space;
    const char *second_space;
    const char *version;
    size_t method_length;
    size_t target_length;
    size_t version_length;

    if (request == NULL || method == NULL || target == NULL ||
        method_capacity == 0U || target_capacity == 0U)
    {
        return false;
    }

    line_end = strstr(request, "\r\n");
    if (line_end == NULL)
    {
        return false;
    }
    first_space = memchr(request, ' ', (size_t)(line_end - request));
    if (first_space == NULL || first_space == request)
    {
        return false;
    }
    second_space = memchr(first_space + 1, ' ', (size_t)(line_end - first_space - 1));
    if (second_space == NULL || second_space == first_space + 1)
    {
        return false;
    }

    method_length = (size_t)(first_space - request);
    target_length = (size_t)(second_space - first_space - 1);
    version = second_space + 1;
    version_length = (size_t)(line_end - version);
    if (method_length >= method_capacity || target_length >= target_capacity ||
        !((version_length == strlen("HTTP/1.0") &&
           memcmp(version, "HTTP/1.0", version_length) == 0) ||
          (version_length == strlen("HTTP/1.1") &&
           memcmp(version, "HTTP/1.1", version_length) == 0)))
    {
        return false;
    }

    memcpy(method, request, method_length);
    method[method_length] = '\0';
    memcpy(target, first_space + 1, target_length);
    target[target_length] = '\0';
    return true;
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

static void numforge_handle_connection(NumForgeSocket socket)
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

    if (!numforge_read_request(
            socket, request, sizeof(request), &length, &has_content_length,
            &origin_allowed) ||
        !numforge_request_target(request, method, sizeof(method), target, sizeof(target)))
    {
        numforge_send_response(socket, 400, "Bad Request", "text/plain; charset=utf-8", "Bad request.\n");
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

static void numforge_open_browser(void)
{
#ifdef _WIN32
    const char *disabled = getenv("NUMFORGE_WEB_NO_BROWSER");

    if (disabled == NULL || strcmp(disabled, "1") != 0)
    {
        (void)ShellExecuteA(NULL, "open", "http://127.0.0.1:8765", NULL, NULL, SW_SHOWNORMAL);
    }
#endif
}

static void numforge_configure_client_socket(NumForgeSocket socket)
{
#ifdef _WIN32
    DWORD timeout = NUMFORGE_WEB_SOCKET_TIMEOUT_MS;

    (void)setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO,
                     (const char *)&timeout, sizeof(timeout));
    (void)setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO,
                     (const char *)&timeout, sizeof(timeout));
#else
    struct timeval timeout;

    timeout.tv_sec = NUMFORGE_WEB_SOCKET_TIMEOUT_MS / 1000;
    timeout.tv_usec = (NUMFORGE_WEB_SOCKET_TIMEOUT_MS % 1000) * 1000;
    (void)setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    (void)setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
#endif
}

/*
------------------------------------------------------------------------------------------------------------------------------
    Local web server entry point.
------------------------------------------------------------------------------------------------------------------------------
*/
int main(void)
{
    NumForgeSocket listener;
    struct sockaddr_in address;
    int reuse_address = 1;

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

    (void)setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse_address, sizeof(reuse_address));
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(NUMFORGE_WEB_PORT);

    if (bind(listener, (const struct sockaddr *)&address, sizeof(address)) != 0 || listen(listener, 8) != 0)
    {
        fputs("NumForge web: port 8765 is unavailable\n", stderr);
        numforge_close_socket(listener);
        numforge_networking_stop();
        return 1;
    }

    puts("NumForge web is running at http://127.0.0.1:8765");
    puts("Press Ctrl+C to stop the local server.");
    numforge_open_browser();

    for (;;)
    {
        NumForgeSocket client = accept(listener, NULL, NULL);

        if (client != NUMFORGE_INVALID_SOCKET)
        {
            numforge_configure_client_socket(client);
            numforge_handle_connection(client);
            numforge_close_socket(client);
        }
    }
}
