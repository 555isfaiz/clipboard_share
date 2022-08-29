#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <pthread.h>
#include <unistd.h>
#include "udp_helper.h"
#include "msg.h"

extern char *TOKEN;

int main(int argc, char *argv[])
{
    int opt;
    while ((opt = getopt(argc, argv, "ht:")) != -1) {
        switch(opt) {
        case 't':
            TOKEN = optarg;
            break;

        case 'h':
        default:
            printf("usage: %s [OPTS] [FILE]\n"
                   "options:\n"
                   " -h      print this help.\n"
                   " -t      set the token for clipboard_share and run.\n",
                   argv[0]);
            return EXIT_SUCCESS;
        }
    }

    udp_init();
    udp_server_init();
    udp_boardcast();

    printf("init done...\n");

    sleep(10);

    return 0;
}