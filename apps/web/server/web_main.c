#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#include "apps/web/server/web_server.h"
#include "apps/web/server/web_workers.h"
#include "apps/web/http/http_request.h"
#include <numforge/runtime.h>

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>
#include <stdatomic.h>
#include <sys/select.h>
#include <unistd.h>
#endif

#define NUMFORGE_WEB_PORT 8765

/* Signal handlers only publish a stop flag. The acceptor owns orderly cleanup. */
#ifdef _WIN32
static volatile LONG stop_requested;
static BOOL WINAPI numforge_stop_signal(DWORD event)
{
    if (event != CTRL_C_EVENT && event != CTRL_BREAK_EVENT) return FALSE;
    (void)InterlockedExchange(&stop_requested, 1);
    return TRUE;
}
static bool numforge_stopping(void) { return InterlockedCompareExchange(&stop_requested, 0, 0) != 0; }
static bool numforge_install_signals(void) { return SetConsoleCtrlHandler(numforge_stop_signal, TRUE) != 0; }
#else
static _Atomic bool stop_requested;
static void numforge_stop_signal(int number)
{
    (void)number; atomic_store_explicit(&stop_requested, true, memory_order_relaxed);
}
static bool numforge_stopping(void) { return atomic_load_explicit(&stop_requested, memory_order_relaxed); }
static bool numforge_install_signals(void)
{
    /* A process signal may arrive on a worker. Only a lock-free atomic is safe
     * both across threads and inside the signal handler. */
    if (!atomic_is_lock_free(&stop_requested)) return false;
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = numforge_stop_signal;
    return sigemptyset(&action.sa_mask) == 0 && sigaction(SIGINT, &action, NULL) == 0 && sigaction(SIGTERM, &action, NULL) == 0;
}
#endif

/* Platform networking and browser-launch helpers. */

static bool numforge_networking_start(
    void
)
{
#ifdef _WIN32
    WSADATA data;

    return WSAStartup(MAKEWORD(2, 2), &data) == 0;
#else
    return signal(SIGPIPE, SIG_IGN) != SIG_ERR;
#endif
}

static void numforge_networking_stop(
    void
)
{
#ifdef _WIN32
    WSACleanup();
#endif
}

static void numforge_open_browser(
    uint16_t port,
    bool enabled
)
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

static bool numforge_configure_client_socket(
    NumForgeSocket socket
)
{
#ifdef _WIN32
    u_long nonblocking = 1UL;

    return ioctlsocket(socket, FIONBIO, &nonblocking) == 0;
#else
    int flags;

    if (socket >= FD_SETSIZE)
    {
        return false;
    }

    flags = fcntl(socket, F_GETFL, 0);

    return flags >= 0 && fcntl(socket, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
}

/* Local web server entry point. */

static bool numforge_parse_port(
    const char *text,
    uint16_t *port
)
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
    bool *open_browser,
    const char **public_origin,
    unsigned int *workers,
    unsigned int *queue
)
{
    int index;

    if (port == NULL || open_browser == NULL || public_origin == NULL)
    {
        return false;
    }

    *port = NUMFORGE_WEB_PORT;
    *open_browser = true;
    *public_origin = NULL;
    *workers = NUMFORGE_WEB_DEFAULT_WORKERS;
    *queue = NUMFORGE_WEB_DEFAULT_QUEUE;

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
        else if (strcmp(argv[index], "--origin") == 0 && index + 1 < argc && *public_origin == NULL)
        {
            *public_origin = argv[++index];
            if (!numforge_http_valid_origin(*public_origin))
            {
                return false;
            }
        }
        else if ((strcmp(argv[index], "--workers") == 0 || strcmp(argv[index], "--queue-capacity") == 0) && index + 1 < argc)
        {
            bool worker_count = strcmp(argv[index], "--workers") == 0;
            uint16_t count;
            if (!numforge_parse_port(argv[++index], &count) ||
                count > (worker_count ? NUMFORGE_WEB_MAX_WORKERS : NUMFORGE_WEB_MAX_QUEUE)) return false;
            if (worker_count) *workers = count;
            else *queue = count;
        }
        else
        {
            return false;
        }
    }

    return true;
}

int main(
    int argc,
    char **argv
)
{
    NumForgeSocket listener;
    struct sockaddr_in address;
    int reuse_address = 1;
    uint16_t port;
    bool open_browser;
    const char *public_origin;
    unsigned int worker_count, queue_capacity;

    if (!numforge_parse_options(argc, argv, &port, &open_browser, &public_origin, &worker_count, &queue_capacity))
    {
        fputs("Usage: numforge_web [--port 1-65535] [--no-browser] [--origin https://host[:port]] [--workers 1-16] [--queue-capacity 1-64]\n", stderr);

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
    if (setsockopt(
            listener, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char *)&reuse_address, sizeof(reuse_address)) !=
        0)
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
    address.sin_addr.s_addr = htonl(UINT32_C(0x7f000001)); /* 127.0.0.1; INADDR_LOOPBACK is not POSIX. */
    address.sin_port = htons(port);

    if (bind(listener, (const struct sockaddr *)&address, sizeof(address)) != 0 ||
        listen(listener, NUMFORGE_WEB_MAX_QUEUE) != 0)
    {
        fprintf(stderr, "NumForge web: port %u is unavailable\n", (unsigned int)port);
        numforge_close_socket(listener);
        numforge_networking_stop();

        return 1;
    }

    if (!numforge_configure_client_socket(listener) || !numforge_install_signals()) {
        fputs("NumForge web: failed to initialize listener control\n", stderr);
        numforge_close_socket(listener); numforge_networking_stop(); return 1;
    }
    bool benchmark_serial = false;
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
    /* Existing per-request phase/live-allocation diagnostics require the same
     * thread for retained values, expiry and final cleanup. Never enable those
     * ledgers in the concurrent production path. */
    const char *trace = getenv("NUMFORGE_BENCH_TRACE");
    benchmark_serial = trace && strcmp(trace, "1") == 0;
#endif
    NumForgeWebWorkers *workers = benchmark_serial ? NULL :
        numforge_web_workers_start(worker_count, queue_capacity, port, public_origin);
    if (!benchmark_serial && !workers) {
        fputs("NumForge web: failed to start workers\n", stderr);
        numforge_close_socket(listener); numforge_networking_stop(); return 1;
    }

    printf("NumForge web is running at http://127.0.0.1:%u\n", (unsigned int)port);
    if (public_origin != NULL)
    {
        printf("Trusted public browser origin: %s (requires a reverse proxy or tunnel)\n", public_origin);
    }
    puts("Press Ctrl+C to stop the local server.");
    printf("Workers: %u; queued connections: %u\n", benchmark_serial ? 1U : worker_count, benchmark_serial ? 0U : queue_capacity);
    numforge_open_browser(port, open_browser);

    int result = 0;
    while (!numforge_stopping())
    {
        numforge_web_workers_expire(workers, numforge_monotonic_ms());
        numforge_web_server_expire(numforge_monotonic_ms());
        fd_set ready;
        struct timeval timeout = {0, 100000L};
        FD_ZERO(&ready); FD_SET(listener, &ready);
#ifdef _WIN32
        int selected = select(0, &ready, NULL, NULL, &timeout);
        if (selected < 0 && WSAGetLastError() == WSAEINTR) continue;
#else
        int selected = select(listener + 1, &ready, NULL, NULL, &timeout);
        if (selected < 0 && errno == EINTR) continue;
#endif
        if (selected < 0) { result = 1; break; }
        if (!selected || numforge_stopping()) continue;
        NumForgeSocket client = accept(listener, NULL, NULL);
        if (client == NUMFORGE_INVALID_SOCKET) {
#ifdef _WIN32
            int error = WSAGetLastError();
            if (error == WSAEWOULDBLOCK || error == WSAEINTR || error == WSAECONNRESET) continue;
#else
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR || errno == ECONNABORTED) continue;
#endif
            fputs("NumForge web: failed to accept connection\n", stderr);
            result = 1; break;
        }

        if (numforge_configure_client_socket(client))
        {
            uint64_t accepted = numforge_monotonic_ms();
            if (benchmark_serial && accepted <= UINT64_MAX - NUMFORGE_WEB_REQUEST_TIMEOUT_MS) {
                numforge_handle_connection_before(client, port, public_origin, accepted + NUMFORGE_WEB_REQUEST_TIMEOUT_MS);
                numforge_close_socket(client); continue;
            }
            if (numforge_web_workers_submit(workers, client, accepted)) continue;
            numforge_reject_connection(client);
        }
        numforge_close_socket(client);
    }
    numforge_close_socket(listener);
    numforge_web_workers_stop(workers);
    if (!numforge_web_server_shutdown()) result = 1;
    numforge_networking_stop();
    return result;
}
