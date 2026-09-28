#ifndef NUMFORGE_WEB_SERVER_H
#define NUMFORGE_WEB_SERVER_H

#include "web_socket.h"

#include <stdint.h>

void numforge_handle_connection(NumForgeSocket socket, uint16_t port);

#endif
