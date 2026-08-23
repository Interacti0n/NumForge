#include <errno.h>
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
#include <sys/socket.h>
#include <unistd.h>

typedef int NumForgeSocket;
#define NUMFORGE_INVALID_SOCKET (-1)
#define numforge_close_socket close
#endif

#include "web_api.h"
#include "web_page.h"

#define NUMFORGE_WEB_PORT 8765
#define NUMFORGE_WEB_REQUEST_CAPACITY 8192U

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

static const char *numforge_find_header_end(const char *request)
{
    return strstr(request, "\r\n\r\n");
}

static bool numforge_read_request(NumForgeSocket socket, char *buffer, size_t capacity, size_t *length)
{
    size_t used = 0U;
    const char *header_end = NULL;
    size_t header_length;
    const char *content_length_text;
    char *end;
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
    content_length_text = strstr(buffer, "Content-Length:");
    if (content_length_text != NULL)
    {
        errno = 0;
        body_length = strtoull(content_length_text + strlen("Content-Length:"), &end, 10);
        if (errno != 0 || end == content_length_text + strlen("Content-Length:") ||
            body_length > capacity - header_length - 1U)
        {
            return false;
        }
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

    *length = used;
    return true;
}

static bool numforge_request_target(const char *request, char *method, size_t method_capacity,
                                    char *target, size_t target_capacity)
{
    if (method_capacity < 16U || target_capacity < 128U)
    {
        return false;
    }

#ifdef _MSC_VER
    return sscanf_s(request, "%15s %127s HTTP/1.", method, (unsigned int)method_capacity,
                    target, (unsigned int)target_capacity) == 2;
#else
    return sscanf(request, "%15s %127s HTTP/1.", method, target) == 2;
#endif
}

static void numforge_handle_evaluation(NumForgeSocket socket, const char *body)
{
    CalculatorError error;
    CalculatorStatus status;
    char *result = NULL;
    char *response;
    size_t response_capacity;

    status = numforge_web_evaluate(body, &result, &error);
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
                                   "{\"ok\":false,\"error\":\"out of memory\"}");
        }
        free(result);
        return;
    }

    response = malloc(192U);
    if (response != NULL)
    {
        (void)snprintf(response, 192U,
                       "{\"ok\":false,\"error\":\"%s at column %zu\"}",
                       calculator_status_to_string(status), error.offset + 1U);
        numforge_send_response(socket, 400, "Bad Request", "application/json; charset=utf-8", response);
        free(response);
    }
    else
    {
        numforge_send_response(socket, 500, "Internal Server Error", "application/json; charset=utf-8",
                               "{\"ok\":false,\"error\":\"out of memory\"}");
    }
}

static void numforge_handle_connection(NumForgeSocket socket)
{
    char request[NUMFORGE_WEB_REQUEST_CAPACITY];
    char method[16];
    char target[128];
    const char *body;
    size_t length;

    if (!numforge_read_request(socket, request, sizeof(request), &length) ||
        !numforge_request_target(request, method, sizeof(method), target, sizeof(target)))
    {
        numforge_send_response(socket, 400, "Bad Request", "text/plain; charset=utf-8", "Bad request.\n");
        return;
    }

    (void)length;
    body = numforge_find_header_end(request);
    if (body != NULL)
    {
        body += 4;
    }

    if (strcmp(method, "GET") == 0 && strcmp(target, "/") == 0)
    {
        numforge_send_response(socket, 200, "OK", "text/html; charset=utf-8", NUMFORGE_WEB_PAGE);
    }
    else if (strcmp(method, "POST") == 0 && strcmp(target, "/api/evaluate") == 0 && body != NULL)
    {
        numforge_handle_evaluation(socket, body);
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
    return true;
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
    (void)ShellExecuteA(NULL, "open", "http://127.0.0.1:8765", NULL, NULL, SW_SHOWNORMAL);
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
            numforge_handle_connection(client);
            numforge_close_socket(client);
        }
    }
}
