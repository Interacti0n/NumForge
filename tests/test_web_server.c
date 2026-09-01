#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>

typedef SOCKET SmokeSocket;
typedef int SmokeSocketLength;
#define SMOKE_INVALID_SOCKET INVALID_SOCKET
#define smoke_close_socket closesocket
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

typedef int SmokeSocket;
typedef socklen_t SmokeSocketLength;
#define SMOKE_INVALID_SOCKET (-1)
#define smoke_close_socket close
#endif

#define SMOKE_RESPONSE_CAPACITY 65536U
#define SMOKE_START_ATTEMPTS 200U
#define SMOKE_RETRY_DELAY_MS 25U

typedef struct SmokeServerProcess
{
#ifdef _WIN32
    PROCESS_INFORMATION information;
#else
    pid_t id;
#endif
    bool started;
} SmokeServerProcess;

static void smoke_sleep(unsigned int milliseconds)
{
#ifdef _WIN32
    Sleep(milliseconds);
#else
    struct timespec duration;

    duration.tv_sec = (time_t)(milliseconds / 1000U);
    duration.tv_nsec = (long)(milliseconds % 1000U) * 1000000L;
    (void)nanosleep(&duration, NULL);
#endif
}

static bool smoke_networking_start(void)
{
#ifdef _WIN32
    WSADATA data;

    return WSAStartup(MAKEWORD(2, 2), &data) == 0;
#else
    return signal(SIGPIPE, SIG_IGN) != SIG_ERR;
#endif
}

static void smoke_networking_stop(void)
{
#ifdef _WIN32
    WSACleanup();
#endif
}

static void smoke_configure_socket(SmokeSocket socket_value)
{
#ifdef _WIN32
    DWORD timeout = 2000U;

    (void)setsockopt(socket_value, SOL_SOCKET, SO_RCVTIMEO,
                     (const char *)&timeout, sizeof(timeout));
    (void)setsockopt(socket_value, SOL_SOCKET, SO_SNDTIMEO,
                     (const char *)&timeout, sizeof(timeout));
#else
    struct timeval timeout;

    timeout.tv_sec = 2;
    timeout.tv_usec = 0;
    (void)setsockopt(socket_value, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    (void)setsockopt(socket_value, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
#endif
}

static bool smoke_choose_port(uint16_t *port)
{
    SmokeSocket socket_value;
    struct sockaddr_in address;
    SmokeSocketLength address_length = (SmokeSocketLength)sizeof(address);

    if (port == NULL)
    {
        return false;
    }

    socket_value = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_value == SMOKE_INVALID_SOCKET)
    {
        return false;
    }

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(0U);

    if (bind(socket_value, (const struct sockaddr *)&address, sizeof(address)) != 0 ||
        getsockname(socket_value, (struct sockaddr *)&address, &address_length) != 0)
    {
        smoke_close_socket(socket_value);
        return false;
    }

    *port = ntohs(address.sin_port);
    smoke_close_socket(socket_value);
    return *port != 0U;
}

static bool smoke_server_start(
    SmokeServerProcess *process,
    const char *executable,
    uint16_t port
)
{
    char port_text[16];

    if (process == NULL || executable == NULL ||
        snprintf(port_text, sizeof(port_text), "%u", (unsigned int)port) <= 0)
    {
        return false;
    }

#ifdef _WIN32
    STARTUPINFOA startup;
    char command[32768];
    int command_length;

    memset(&startup, 0, sizeof(startup));
    memset(&process->information, 0, sizeof(process->information));
    startup.cb = sizeof(startup);
    command_length = snprintf(command, sizeof(command), "\"%s\" --port %s --no-browser",
                              executable, port_text);
    if (command_length <= 0 || (size_t)command_length >= sizeof(command) ||
        !CreateProcessA(NULL, command, NULL, NULL, FALSE, CREATE_NO_WINDOW,
                        NULL, NULL, &startup, &process->information))
    {
        return false;
    }
#else
    process->id = fork();
    if (process->id < 0)
    {
        return false;
    }
    if (process->id == 0)
    {
        execl(executable, executable, "--port", port_text, "--no-browser", (char *)NULL);
        _exit(127);
    }
#endif

    process->started = true;
    return true;
}

static bool smoke_server_is_running(SmokeServerProcess *process)
{
    if (process == NULL || !process->started)
    {
        return false;
    }

#ifdef _WIN32
    {
        DWORD exit_code;

        return GetExitCodeProcess(process->information.hProcess, &exit_code) != 0 &&
               exit_code == STILL_ACTIVE;
    }
#else
    {
        int status;
        pid_t result = waitpid(process->id, &status, WNOHANG);

        if (result == 0)
        {
            return true;
        }
        process->started = false;
        return false;
    }
#endif
}

static void smoke_server_stop(SmokeServerProcess *process)
{
    if (process == NULL || !process->started)
    {
        return;
    }

#ifdef _WIN32
    (void)TerminateProcess(process->information.hProcess, 0U);
    (void)WaitForSingleObject(process->information.hProcess, 5000U);
    CloseHandle(process->information.hThread);
    CloseHandle(process->information.hProcess);
#else
    (void)kill(process->id, SIGTERM);
    (void)waitpid(process->id, NULL, 0);
#endif

    process->started = false;
}

static SmokeSocket smoke_connect(uint16_t port)
{
    SmokeSocket socket_value = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in address;

    if (socket_value == SMOKE_INVALID_SOCKET)
    {
        return SMOKE_INVALID_SOCKET;
    }

    smoke_configure_socket(socket_value);
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(port);

    if (connect(socket_value, (const struct sockaddr *)&address, sizeof(address)) != 0)
    {
        smoke_close_socket(socket_value);
        return SMOKE_INVALID_SOCKET;
    }

    return socket_value;
}

static bool smoke_wait_until_ready(SmokeServerProcess *process, uint16_t port)
{
    size_t attempt;

    for (attempt = 0U; attempt < SMOKE_START_ATTEMPTS; attempt++)
    {
        SmokeSocket socket_value;

        if (!smoke_server_is_running(process))
        {
            return false;
        }

        socket_value = smoke_connect(port);
        if (socket_value != SMOKE_INVALID_SOCKET)
        {
            smoke_close_socket(socket_value);
            return true;
        }

        smoke_sleep(SMOKE_RETRY_DELAY_MS);
    }

    return false;
}

static bool smoke_send_all(SmokeSocket socket_value, const char *data, size_t length)
{
    while (length > 0U)
    {
        int sent = send(socket_value, data,
                        (int)(length > 32767U ? 32767U : length), 0);

        if (sent <= 0)
        {
            return false;
        }

        data += (size_t)sent;
        length -= (size_t)sent;
    }

    return true;
}

static bool smoke_exchange(
    uint16_t port,
    const char *request,
    char *response,
    size_t response_capacity
)
{
    SmokeSocket socket_value;
    size_t used = 0U;
    bool succeeded = false;

    if (request == NULL || response == NULL || response_capacity < 2U)
    {
        return false;
    }

    socket_value = smoke_connect(port);
    if (socket_value == SMOKE_INVALID_SOCKET)
    {
        return false;
    }

    if (!smoke_send_all(socket_value, request, strlen(request)))
    {
        smoke_close_socket(socket_value);
        return false;
    }

    while (used < response_capacity - 1U)
    {
        int received = recv(socket_value, response + used,
                            (int)(response_capacity - used - 1U), 0);

        if (received == 0)
        {
            succeeded = true;
            break;
        }
        if (received < 0)
        {
            break;
        }

        used += (size_t)received;
    }

    response[used] = '\0';
    smoke_close_socket(socket_value);
    return succeeded;
}

static bool smoke_expect_response(
    uint16_t port,
    const char *request,
    const char *status,
    const char *body_fragment
)
{
    char response[SMOKE_RESPONSE_CAPACITY];

    if (!smoke_exchange(port, request, response, sizeof(response)))
    {
        fprintf(stderr, "HTTP exchange failed for expected status %s\n", status);
        return false;
    }
    if (strstr(response, status) == NULL)
    {
        fprintf(stderr, "Expected HTTP status %s, received:\n%.500s\n", status, response);
        return false;
    }
    if (body_fragment != NULL && strstr(response, body_fragment) == NULL)
    {
        fprintf(stderr, "Expected response fragment %s, received:\n%.500s\n",
                body_fragment, response);
        return false;
    }

    return true;
}

int main(int argc, char **argv)
{
    static const char get_page[] =
        "GET /?lang=en HTTP/1.1\r\n"
        "Host: 127.0.0.1\r\n"
        "Connection: close\r\n\r\n";
    static const char evaluate[] =
        "POST /api/evaluate?precision=0 HTTP/1.1\r\n"
        "Host: 127.0.0.1\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 6\r\n"
        "Connection: close\r\n\r\n"
        "1E3+24";
    static const char missing_length[] =
        "POST /api/evaluate HTTP/1.1\r\n"
        "Host: 127.0.0.1\r\n"
        "Connection: close\r\n\r\n";
    static const char foreign_origin[] =
        "POST /api/evaluate HTTP/1.1\r\n"
        "Host: 127.0.0.1\r\n"
        "Origin: https://example.com\r\n"
        "Content-Length: 1\r\n"
        "Connection: close\r\n\r\n"
        "1";
    static const char unknown_route[] =
        "GET /missing HTTP/1.1\r\n"
        "Host: 127.0.0.1\r\n"
        "Connection: close\r\n\r\n";
    SmokeServerProcess process;
    char allowed_origin[256];
    uint16_t port = 0U;
    int exit_code = 1;

    memset(&process, 0, sizeof(process));
    if (argc != 2)
    {
        fputs("Usage: web_server_smoke_tests path-to-numforge_web\n", stderr);
        return 2;
    }
    if (!smoke_networking_start())
    {
        fputs("Failed to initialize client networking\n", stderr);
        return 1;
    }
    if (!smoke_choose_port(&port))
    {
        fputs("Failed to select a loopback test port\n", stderr);
        goto cleanup;
    }
    if (!smoke_server_start(&process, argv[1], port) ||
        !smoke_wait_until_ready(&process, port))
    {
        fputs("numforge_web did not start on the selected loopback port\n", stderr);
        goto cleanup;
    }

    if (snprintf(
            allowed_origin, sizeof(allowed_origin),
            "POST /api/evaluate HTTP/1.1\r\n"
            "Host: 127.0.0.1\r\n"
            "Origin: http://127.0.0.1:%u\r\n"
            "Content-Length: 1\r\n"
            "Connection: close\r\n\r\n"
            "1",
            (unsigned int)port
        ) <= 0)
    {
        fputs("Failed to construct same-origin HTTP request\n", stderr);
        goto cleanup;
    }

    if (!smoke_expect_response(port, get_page, "HTTP/1.1 200 OK", "NumForge") ||
        !smoke_expect_response(port, evaluate, "HTTP/1.1 200 OK",
                               "{\"ok\":true,\"result\":\"1024\"}") ||
        !smoke_expect_response(port, allowed_origin, "HTTP/1.1 200 OK",
                               "{\"ok\":true,\"result\":\"1\"}") ||
        !smoke_expect_response(port, missing_length,
                               "HTTP/1.1 411 Length Required", "Content-Length is required") ||
        !smoke_expect_response(port, foreign_origin,
                               "HTTP/1.1 403 Forbidden", "forbidden origin") ||
        !smoke_expect_response(port, unknown_route,
                               "HTTP/1.1 404 Not Found", "Not found"))
    {
        goto cleanup;
    }

    puts("numforge_web end-to-end smoke test passed");
    exit_code = 0;

cleanup:
    smoke_server_stop(&process);
    smoke_networking_stop();
    return exit_code;
}
