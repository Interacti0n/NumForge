#include "web_server.h"

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
#include <sys/select.h>
#include <unistd.h>
#endif

#define NUMFORGE_WEB_PORT 8765

/*
------------------------------------------------------------------------------------------------------------------------------
    Platform networking and browser-launch helpers.
------------------------------------------------------------------------------------------------------------------------------
*/

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

/*
------------------------------------------------------------------------------------------------------------------------------
    Local web server entry point.
------------------------------------------------------------------------------------------------------------------------------
*/

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
            if (numforge_configure_client_socket(client))
            {
                numforge_handle_connection(client, port);
            }

            numforge_close_socket(client);
        }
    }
}
