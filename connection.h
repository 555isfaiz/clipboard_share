#ifndef __UDP_HELPER__
#define __UDP_HELPER__

#ifdef _WIN32
#include <winsock.h>
#else
#include <netinet/in.h>
#endif


int udp_init();

void udp_broadcast();

void broadcast_to_known(char *buf, int len);

int udp_send_as_client(struct sockaddr_in addr, char *buffer, int size);

void handle_datagram(char *buf, int len, struct sockaddr_in from_addr);

int udp_server_init();

int udp_close();

#endif