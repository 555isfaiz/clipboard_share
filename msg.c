#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "msg.h"

char *TOKEN;

/*
 * Hash function by
 * http://www.cse.yorku.ca/~oz/hash.html
 */
unsigned int hash(char *str)
{
    unsigned int hash = 5381;
    int c;
    while ((c = *str++) != 0)
        hash = ((hash << 5) + hash) + c;

    return hash;
}

void set_token(char *str)
{
    unsigned int hash_c = hash(str);
    char buf[32] = {0};
    int n = sprintf(buf, "%d", hash_c);
    TOKEN = calloc(n, sizeof(char));
    memcpy(TOKEN, buf, n);
}

int get_msg_online(char *buf)
{
    strcpy(buf, TOKEN);
    strcpy(buf + sizeof(TOKEN), "o");
    return sizeof(TOKEN) + 1;
}