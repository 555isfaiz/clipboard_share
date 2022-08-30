#ifndef __HLINUX__
#define __HLINUX__

char* read_local_clipboard(int *len);

void write_local_clipboard(char *buf, int len);

void clipboard_monitor_loop();

#endif