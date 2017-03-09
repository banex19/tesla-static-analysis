#ifndef SERVER_HANDLER_H
#define SERVER_HANDLER_H

#include <stdio.h>

#include "protocol.h"

void handle_connection(int fd);

uint16_t get_request(int fd);

#endif
