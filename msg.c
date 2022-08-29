#include <string.h>
#include "msg.h"

char *TOKEN;

int get_msg_online(char *buf)
{
    strcpy(buf, "hello");
    return 5;
}