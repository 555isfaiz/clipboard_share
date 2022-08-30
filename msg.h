#ifndef __MSG__
#define __MSG__

void set_token(char *str);

int gen_msg_online(char *buf);

int gen_msg_ack_online(char *buf);

int gen_msg_clipboard_update(char *buf);

#endif