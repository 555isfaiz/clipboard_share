#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/input.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xfixes.h>
#include "linux.h"
#include "msg.h"
#include "udp_helper.h"

int write_bit = 0;

// wrote read_local_clipboard() and write_local_clipboard() before I was awared of X11 apis.
// maybe use x11 api to rewrite them in the future?
char* read_local_clipboard(int *len)
{
    int fds[2]; 
    char *buf;
    pipe(fds);
    int pid = fork(), status = 0;
    if (pid == 0)
    {
        while ((dup2(fds[1], STDOUT_FILENO) == -1) && (errno == EINTR)) {}
        close(fds[0]);
        close(fds[1]);
        char* argument_list[] = {"--clipboard", NULL};
        execvp("xsel", argument_list);
        exit(1);
    }
    else
    {
        close(fds[1]);
        buf = calloc(4096, 1);
        int read_len = read(fds[0], buf, 4096);
        *len = read_len;
        close(fds[0]);
        waitpid(pid, &status, 0);
    }

    return buf;
}

void write_local_clipboard(char *buf, int len)
{
    write_bit = 1;
    int fds[2]; 
    pipe(fds);
    int pid = fork(), status = 0;
    if (pid == 0)
    {
        while ((dup2(fds[0], STDIN_FILENO) == -1) && (errno == EINTR)) {}
        close(fds[0]);
        close(fds[1]);
        char* argument_list[] = {"--input", "--clipboard", NULL};
        execvp("xsel", argument_list);
        exit(1);
    }
    else
    {
        close(fds[0]);
        write(fds[1], buf, len);
        close(fds[1]);
        waitpid(pid, &status, 0);
    }
}

// https://stackoverflow.com/questions/8755471/x11-wait-for-and-get-clipboard-text
void clipboard_monitor_loop()
{
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        perror("Failed to open X display\n");
        return;
    }

    Atom clipboard = XInternAtom(dpy, "CLIPBOARD", False);

    int event_base, error_base;
    XEvent event;

    if (!XFixesQueryExtension(dpy, &event_base, &error_base))
    {
        perror("XFixesQueryExtension failed\n");
        return;
    }
    XFixesSelectSelectionInput(dpy, DefaultRootWindow(dpy), clipboard, XFixesSetSelectionOwnerNotifyMask);

    while (True)
    {
        XNextEvent(dpy, &event);

        if (write_bit)
        {
            write_bit = 0;
            continue;
        }

        if (event.type == event_base + XFixesSelectionNotify &&
            ((XFixesSelectionNotifyEvent *)&event)->selection == clipboard)
        {
            int cb_len = 0;
            char *cb_buf = read_local_clipboard(&cb_len);
            char send_buf[8192] = {0};
            int msg_len = gen_msg_clipboard_update(send_buf);
            memcpy(send_buf + msg_len, cb_buf, cb_len);
            udp_broadcast_to_known(send_buf, msg_len + cb_len);
            free(cb_buf);
        }
    }
}
