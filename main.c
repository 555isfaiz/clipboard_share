#include "clipboard.h"
#include "connection.h"
#include "msg.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>

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
uint8_t is_verbose = 0;
int is_gbk = 0;
unsigned buffer_size = 1024 * 1024;

void print_usage(char *proc)
{
    printf("usage: %s [OPTS]\n"
           "options:\n"
           " -h      print this help.\n"
           " -t      set the token for clipboard_share and run.\n"
           " -p      set the port for udp server.\n"
           " -v      run in verbose mode."
           " -b      set the maximum buffer size (kb) for clipboard.\n"
           " -g      denotes whether local machine is using gbk encoding.\n",
           proc);
}

int main(int argc, char *argv[])
{
    int opt, has_token = 0, ret;
    while ((opt = getopt(argc, argv, "ht:p:b:gv")) != -1)
    {
        switch (opt)
        {
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

        case 'v':
            is_verbose = 1;
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
        error("No token set...\n");
        print_usage(argv[0]);
        return 0;
    }

    info("init udp server... \n");
    ret = udp_init();
    if (ret < 0)
        return ret;
    ret = udp_server_init();
    if (ret < 0)
        return ret;

    info("doing udp broadcast... \n");
    udp_broadcast();

    info("init done...\n");
    info("start monitor...\n");

    clipboard_monitor_loop();
    return 0;
}
