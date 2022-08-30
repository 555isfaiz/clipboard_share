#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <pthread.h>
#include <unistd.h>
#include "udp_helper.h"
#include "msg.h"

#ifdef _WIN32
// TODO
#elif __linux__
#include "linux.h"
#elif __APPLE__
#include "mac.h"
#endif

extern char *TOKEN;

void print_usage(char *proc)
{
    printf("usage: %s [OPTS]\n"
                   "options:\n"
                   " -h      print this help.\n"
                   " -t      set the token for clipboard_share and run.\n",
                   proc);
}

int main(int argc, char *argv[])
{
    int opt, has_token = 0, ret;
    while ((opt = getopt(argc, argv, "ht:")) != -1) {
        switch(opt) {
        case 't':
            has_token = 1;
            set_token(optarg);
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