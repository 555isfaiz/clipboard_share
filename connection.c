#include "connection.h"
#include "clipboard.h"
#include "msg.h"
#include "utils.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <process.h>
#include <windows.h>
#include <winsock.h>
#pragma comment(lib, "wsock32.lib")
// #include "win.h"
#elif __linux__
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#elif __APPLE__
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#define STREAM_TAG "STREAM_"
#define STREAM_SLICE_LEN 1024

int SERVER_PORT = 53338;
int STREAM_PORT = 53335;

int udp_client_socket = 0;
int udp_server_socket = 0;

struct sockaddr_in *addr_list;
int addr_list_size = 8;
int addr_list_ptr = 0;

extern unsigned buffer_size;
char *buffer_;

void add_to_addr_list(struct sockaddr_in *addr)
{
    for (int i = 0; i < addr_list_ptr; i++)
    {
        if (addr_list[i].sin_addr.s_addr == addr->sin_addr.s_addr && addr_list[i].sin_port == addr->sin_port)
        {
            // already in the list
            return;
        }
    }

    if (addr_list_ptr + 1 == addr_list_size)
    {
        addr_list_size <<= 1;
        struct sockaddr_in *ptr = calloc(addr_list_size, sizeof(struct sockaddr_in));
        memcpy(ptr, addr_list, (addr_list_ptr + 1) * sizeof(struct sockaddr_in));
        free(addr_list);
        addr_list = ptr;
    }

    memcpy(addr_list + addr_list_ptr++, addr, sizeof(struct sockaddr_in));
    addr_list[addr_list_ptr - 1].sin_port = htons(SERVER_PORT);
}

void remove_from_addr_list(struct sockaddr_in addr)
{
    for (int i = 0; i < addr_list_ptr; i++)
    {
        if (addr_list[i].sin_addr.s_addr == addr.sin_addr.s_addr && addr_list[i].sin_port == addr.sin_port)
        {
            memcpy(&(addr_list[i]), &(addr_list[i + 1]), (addr_list_ptr - i - 1) * sizeof(struct sockaddr_in));
            addr_list_ptr--;
            break;
        }
    }
}

int udp_init()
{
#ifdef _WIN32
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
    {
        return 0;
    }
#endif

    udp_client_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (0 > udp_client_socket)
    {
        perror("create client socket failed\n");
        return udp_client_socket;
    }

    return 0;
}

void udp_broadcast()
{
    int on = 1;
    int ret = setsockopt(udp_client_socket, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on));
    if (ret < 0)
    {
        perror("setsockopt broadcast error");
        return;
    }

    char buf[128] = {0};
    int len = gen_msg_online(buf);
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    addr.sin_port = htons(SERVER_PORT);
    ret = udp_send_as_client(addr, buf, len);
    if (ret < 0)
    {
        perror("udp broadcast send error");
        return;
    }

    int off = 0;
    ret = setsockopt(udp_client_socket, SOL_SOCKET, SO_BROADCAST, &off, sizeof(off));
}

int tcp_stream_send(struct sockaddr_in addr, char *buffer, int size)
{
    int sockfd, connfd, ret, send_num = 0;
    unsigned int len = 0;
    struct sockaddr_in servaddr, cli;

    // socket create and verification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        perror("TCP socket create failed..");
        return -1;
    }

    bzero(&servaddr, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(STREAM_PORT);

    if ((bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
    {
        perror("TCP bind failed...");
        close(sockfd);
        return -1;
    }

    if ((listen(sockfd, 5)) != 0)
    {
        perror("TCP listen failed...");
        close(sockfd);
        return -1;
    }

    len = sizeof(cli);

    char tmp[64] = {0};
    int ptr = gen_msg_stream(tmp);
    *((uint32_t *)(tmp + ptr)) = size;
    ret = sendto(udp_client_socket, tmp, sizeof(tmp), 0, (struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0)
    {
        remove_from_addr_list(addr);
        perror("udp client send error:");
        close(sockfd);
        return ret;
    }

    connfd = accept(sockfd, (struct sockaddr *)&cli, &len);
    if (connfd < 0)
    {
        perror("TCP accept failed...");
        close(sockfd);
        return -1;
    }

    while (send_num < size)
    {
        int tosend = size - send_num > STREAM_SLICE_LEN ? STREAM_SLICE_LEN : size - send_num;
        ret = write(connfd, buffer + send_num, tosend);
        if (ret < 0)
        {
            remove_from_addr_list(addr);
            perror("TCP send error:");
            close(sockfd);
            return ret;
        }
        send_num += ret;
    }

    close(sockfd);
    return 0;
}

int tcp_stream_recv(struct sockaddr_in from_addr, int size)
{
    int sockfd, recv_num, ret;
    struct sockaddr_in servaddr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        perror("TCP recv socket create failed...\n");
        return -1;
    }

    bzero(&servaddr, sizeof(servaddr));

    servaddr.sin_family = from_addr.sin_family;
    servaddr.sin_addr = from_addr.sin_addr;
    servaddr.sin_port = htons(STREAM_PORT);

    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) != 0)
    {
        perror("TCP recv connect failed...\n");
        close(sockfd);
        return -1;
    }

    while (recv_num < size)
    {
        int torecv = size - recv_num > STREAM_SLICE_LEN ? STREAM_SLICE_LEN : size - recv_num;
        ret = read(sockfd, buffer_ + recv_num, torecv);
        if (ret < 0)
        {
            remove_from_addr_list(from_addr);
            perror("TCP recv error:");
            close(sockfd);
            return ret;
        }
        recv_num += ret;
    }

    handle_datagram(buffer_, size, from_addr);

    close(sockfd);
    return 0;
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

int send_payload(struct sockaddr_in addr, char *buffer, int size)
{
    if (size > STREAM_SLICE_LEN)
        return tcp_stream_send(addr, buffer, size);
    else
        return udp_send_as_client(addr, buffer, size);
}

void broadcast_to_known(char *buf, int len)
{
    for (int i = 0; i < addr_list_ptr; i++)
    {
        int ret = send_payload(addr_list[i], buf, len);
        if (ret < 0)
            break; // error happened, this addr has been removed
    }
}

void handle_datagram(char *buf, int len, struct sockaddr_in from_addr)
{
    char buffer[64] = {0};

    // msg online: other device is online
    int buf_len = gen_msg_online(buffer);
    if (strncmp(buf, buffer, len) == 0)
    {
        info("new connection from %s\n", inet_ntoa(from_addr.sin_addr));
        from_addr.sin_port = htons(SERVER_PORT);
        add_to_addr_list(&from_addr);
        buf_len = gen_msg_ack_online(buffer);
        udp_send_as_client(from_addr, buffer, buf_len);
        return;
    }

    // msg ack online: record responded devices
    buf_len = gen_msg_ack_online(buffer);
    if (strncmp(buf, buffer, len) == 0)
    {
        debug("ack online from %s\n", inet_ntoa(from_addr.sin_addr));
        add_to_addr_list(&from_addr);
        return;
    }

    // msg clipboard update: update local clipboard with content from other device
    buf_len = gen_msg_clipboard_update(buffer);
    if (strncmp(buf, buffer, buf_len) == 0)
    {
        debug("new clipboard content: %s\n", buf + buf_len);
        write_local_clipboard(buf + buf_len, len - buf_len);
    }

    // msg stream: set up a tcp stream connection for big payloads
    buf_len = gen_msg_stream(buffer);
    if (strncmp(buf, buffer, buf_len) == 0)
    {
        debug("preparing tcp recv. len: %d\n", *((uint32_t *)(buf + buf_len)));
        tcp_stream_recv(from_addr, *((uint32_t *)(buf + buf_len)));
    }
}

void *server_loop()
{
    struct sockaddr_in sendaddr;
    unsigned int len = sizeof(sendaddr);

#ifdef _WIN32
    WSADATA wsaData;
    char name[255];
    PHOSTENT hostinfo;

    if (WSAStartup(MAKEWORD(2, 0), &wsaData) == 0)
    {
        if (gethostname(name, sizeof(name)) == 0)
        {
            if ((hostinfo = gethostbyname(name)) == NULL)
            {
                perror("can't get local ip");
                return (void *)0;
            }
        }
        WSACleanup();
    }
#else
    struct ifaddrs *ifaddr, *ifa;

    if (getifaddrs(&ifaddr) == -1)
    {
        perror("getifaddrs");
        return (void *)0;
    }
#endif

    while (1)
    {
    lo_start:;
        int ret = recvfrom(udp_server_socket, buffer_, buffer_size, 0, (struct sockaddr *)&sendaddr, &len);
        if (ret < 0)
        {
            perror("recvfrom error");
            break;
        }
#ifdef _WIN32
        if ((*(struct in_addr *)*hostinfo->h_addr_list).s_addr == sendaddr.sin_addr.s_addr)
        {
            goto lo_start;
        }
#else
        for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
        {
            if (ifa->ifa_addr == NULL)
                continue;

            if (ifa->ifa_addr->sa_family == AF_INET &&
                (((struct sockaddr_in *)ifa->ifa_addr)->sin_addr.s_addr == sendaddr.sin_addr.s_addr))
            {
                goto lo_start;
            }
        }
#endif
        handle_datagram(buffer_, ret, sendaddr);
        memset(buffer_, 0, buffer_size);
    }

#ifndef _WIN32
    freeifaddrs(ifaddr);
#endif

    return (void *)0;
}

int udp_server_init()
{
    // init addr list
    addr_list = calloc(addr_list_size, sizeof(struct sockaddr_in));

    udp_server_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (0 > udp_server_socket)
    {
        perror("create server socket failed\n");
        return udp_server_socket;
    }

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
#ifdef _WIN32
    _beginthread(server_loop, 0, NULL);
#else
    pthread_t ntid;
    int err;

    err = pthread_create(&ntid, NULL, server_loop, NULL);
    if (err != 0)
    {
        perror("can't create thread for udp server");
        return err;
    }
#endif

    buffer_ = calloc(buffer_size, sizeof(char));
    if (!buffer_)
    {
        perror("failed to create buffer:");
        return -1;
    }

    return 0;
}

int udp_close()
{
    close(udp_client_socket);
    close(udp_server_socket);
    free(buffer_);
    return 0;
}
