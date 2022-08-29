#ifndef __UDP_HELPER__
#define __UDP_HELPER__

#include <netinet/in.h>

int udp_init();

void udp_boardcast();

int udp_send_as_client(struct sockaddr_in addr, char *buffer, int size);

int udp_server_init();

int udp_close();

#endif