#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <pthread.h>
#include <unistd.h>
#include "udp_helper.h"
#include "msg.h"

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
    int opt, has_token = 0;
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
    udp_init();
    udp_server_init();
    printf("ok\n");

    printf("doing udp broadcast... ");
    udp_broadcast();
    printf("ok\n");

    printf("init done...\n");

    sleep(10);

    return 0;
}