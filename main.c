#include <stdlib.h>
#include <stdio.h>
#include "connection.h"
#include "msg.h"
#include "clipboard.h"

#ifdef _WIN32
#include "getopt_win.h"
// #include "win.h"
#elif __linux__
// #include "linux.h"
#include <getopt.h>
#elif __APPLE__
// #include "mac.h"
#include <getopt.h>
#endif

extern int SERVER_PORT;
int is_gbk = 0;
unsigned buffer_size = 1024 * 1024;

void print_usage(char *proc)
{
    printf("usage: %s [OPTS]\n"
                   "options:\n"
                   " -h      print this help.\n"
                   " -t      set the token for clipboard_share and run.\n"
                   " -p      set the port for udp server.\n"
                   " -b      set the maximum buffer size (kb) for clipboard.\n"
                   " -g      denotes whether local machine is using gbk encoding.\n",
                   proc);
}

int main(int argc, char *argv[])
{
    int opt, has_token = 0, ret;
    while ((opt = getopt(argc, argv, "ht:p:b:g")) != -1) {
        switch(opt) {
        case 't':
            has_token = 1;
            set_token(optarg);
            break;

        case 'p':
            SERVER_PORT = atoi(optarg);
            break;

        case 'g':
            is_gbk = 1;
            break;

        case 'b':
            buffer_size = atoi(optarg) * 1024;
            break;

        case 'h':
        default:
            print_usage(argv[0]);
            return 0;
        }
    }

    if (!has_token) 
    {
        printf("No token set...");
        print_usage(argv[0]);
        return 0;
    }

    printf("init udp server... ");
    ret = udp_init();
    if (ret < 0) return ret;
    ret = udp_server_init();
    if (ret < 0) return ret;
    printf("ok\n");

    printf("doing udp broadcast... ");
    udp_broadcast();
    printf("ok\n");

    printf("init done...\n");
    printf("start monitor...\n");

    clipboard_monitor_loop();
    return 0;
}