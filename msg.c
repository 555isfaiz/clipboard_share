#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "msg.h"

char *TOKEN;
int TOKEN_LEN;

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
    TOKEN_LEN = n;
    memcpy(TOKEN, buf, n);
}

int gen_msg_online(char *buf)
{
    strncpy(buf, TOKEN, TOKEN_LEN);
    strcpy(buf + TOKEN_LEN, "o");
    return TOKEN_LEN + 1;
}

int gen_msg_clipboard_update(char *buf)
{
    strncpy(buf, TOKEN, TOKEN_LEN);
    strcpy(buf + TOKEN_LEN, "c");
    return TOKEN_LEN + 1;
}