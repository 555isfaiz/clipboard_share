#include <stdlib.h>
#include <getopt.h>
#include <server.c>

char *TOKEN;

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



    return 0;
}