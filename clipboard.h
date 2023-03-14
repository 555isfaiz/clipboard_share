#ifndef __HCLIPBOARD__
#define __HCLIPBOARD__

#define CB_TYPE_TEXT 101u
#define CB_TYPE_IMAGE 102u

void read_local_clipboard(int *len);

void write_local_clipboard(char *buf, int len);

void clipboard_monitor_loop();

#endif