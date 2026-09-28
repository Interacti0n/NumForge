#ifndef NUMFORGE_WEB_SOCKET_H
#define NUMFORGE_WEB_SOCKET_H

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>

typedef SOCKET NumForgeSocket;
#define NUMFORGE_INVALID_SOCKET INVALID_SOCKET
#define numforge_close_socket closesocket
#else
#include <sys/socket.h>

typedef int NumForgeSocket;
#define NUMFORGE_INVALID_SOCKET (-1)
#define numforge_close_socket close
#endif

#endif
