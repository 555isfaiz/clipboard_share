#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <ifaddrs.h>
#include <unistd.h>
#include <pthread.h>
#include "msg.h"
#include "udp_helper.h"

int SERVER_PORT = 8888;
int CLIENT_PORT = 9999;

int udp_client_socket = 0;
int udp_server_socket = 0;

int udp_init()
{
	udp_client_socket = socket(AF_INET, SOCK_DGRAM, 0);
	if(0 > udp_client_socket)
	{
		perror("create client socket failed\n");
		return udp_client_socket;
	}

	udp_server_socket = socket(AF_INET, SOCK_DGRAM, 0);
	if(0 > udp_server_socket)
	{
		perror("create server socket failed\n");
		return udp_server_socket;
	}

	return 0;
}

void udp_boardcast()
{
	int on=1;
	int ret = setsockopt(udp_client_socket, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on));
	if (ret < 0)
	{
		perror("setsockopt broadcast error");
		return;
	}

	struct ifaddrs *ifaddr, *ifa;

	if (getifaddrs(&ifaddr) == -1)
	{
		perror("getifaddrs");
		return;
	}

	char buf[1024] = {0};
	int len = get_msg_online(buf);
	for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
	{
		if (ifa->ifa_addr == NULL)
			continue;

		if (ifa->ifa_addr->sa_family == AF_INET)
		{
			struct sockaddr_in addr;
			addr.sin_family = AF_INET;
			addr.sin_addr = ((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
			addr.sin_port = htons(SERVER_PORT);
			int ret = udp_send_as_client(addr, buf, len);
			if (ret < 0) 
			{
				perror("udp broadcast send error");
				return;
			}
		}
	}

	freeifaddrs(ifaddr);

	int off = 0;
	ret = setsockopt(udp_client_socket, SOL_SOCKET, SO_BROADCAST, &off, sizeof(off));
}

int udp_send_as_client(struct sockaddr_in addr, char *buffer, int size)
{
	int ret, send_num = 0;
	while (send_num < size)
	{
		ret = sendto(udp_client_socket, buffer, size, 0, (struct sockaddr *)&addr, sizeof(addr));
		if (ret < 0)
		{
			perror("udp client send error:");
			return ret;
		}
		send_num += ret; 
	}
	return 0;
}

void *server_loop()
{
	char buffer[128]={0};
	struct sockaddr_in sendaddr;
	socklen_t len = sizeof(sendaddr);

	while (1)
	{
		int ret  = recvfrom(udp_server_socket, buffer, sizeof(buffer), 0, (struct sockaddr*)&sendaddr, &len);
		if(ret < 0)
		{
			perror("recvfrom error");
			break;
		}
		printf(buffer);
		printf("\n");
	}

	return (void*)0;
}

int udp_server_init()
{
	// bind socket
	struct sockaddr_in addr_serv;
	addr_serv.sin_family = AF_INET;
	addr_serv.sin_port = htons(SERVER_PORT);
	addr_serv.sin_addr.s_addr = htonl(INADDR_ANY);
	int ret = bind(udp_server_socket, (struct sockaddr *)&addr_serv, sizeof(addr_serv));
	if (ret < 0)
	{
		perror("bind error:");
		return ret;
	}

	// start thread
	pthread_t ntid;
	int err;

	err = pthread_create(&ntid, NULL, server_loop, NULL);
	if(err != 0)
	{
		perror("can't create thread for udp server");
		return err;
	}

	return 0;
}

int udp_close()
{
	return 0;
}