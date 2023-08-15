#include "clipboard.h"
#include "connection.h"
#include "msg.h"
#include "utils.h"
#include <X11/Xlib.h>
#include <X11/extensions/Xfixes.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define PLAINTEXT "UTF8_STRING"
#define IMAGEPNG "image/png"

int write_bit = 0;
extern unsigned buffer_size;
char *cb_buffer_;
uint8_t is_wayland;

void read_fork(const char *command, char *const *arg, char *buffer, int len, int *len_out)
{
    int fds[2];
    pipe(fds);
    int pid = fork(), status = 0;
    if (pid == 0)
    {
        while ((dup2(fds[1], STDOUT_FILENO) == -1) && (errno == EINTR))
        {
        }
        close(fds[0]);
        close(fds[1]);
        execvp(command, arg);
        exit(1);
    }
    else
    {
        close(fds[1]);
        int off = 0, nread;
        while ((nread = read(fds[0], buffer + off, len - off)) > 0)
            off += nread;

        if (nread < 0)
            perror("error reading from fork ");

        *len_out = off;
        close(fds[0]);
        waitpid(pid, &status, 0);
    }
}

uint8_t get_clipboard_type(char *type_out)
{
    int len = 0;
    if (is_wayland)
    {
        char *const argument_list[] = {"wl-paste", "-l", NULL};
        read_fork("wl-paste", argument_list, cb_buffer_, buffer_size, &len); // no need to use a new buffer here
    }
    else
    {
        char *const argument_list[] = {"xclip", "-selection", "clipboard", "-t", "TARGETS", "-o", NULL};
        read_fork("xclip", argument_list, cb_buffer_, buffer_size, &len); // no need to use a new buffer here
    }
    if (len <= 0)
        return 0;

    if (strstr(cb_buffer_, IMAGEPNG))
    {
        strcpy(type_out, IMAGEPNG);
        return CB_TYPE_IMAGE;
    }
    else if (strstr(cb_buffer_, PLAINTEXT))
    {
        strcpy(type_out, PLAINTEXT);
        return CB_TYPE_TEXT;
    }
    return 0;
}

// wrote read_local_clipboard() and write_local_clipboard() before I was awared of X11 apis.
// maybe use x11 api to rewrite them in the future?
void read_local_clipboard(int *len)
{
    char type[16];
    uint8_t type_code = get_clipboard_type(type);
    if (!type_code)
        return;

    int msg_len = gen_msg_clipboard_update(cb_buffer_);

    // write type code
    (cb_buffer_)[msg_len] = (char)type_code;
    msg_len++;

    if (is_wayland)
    {
        char *const argument_list[] = {"wl-paste", NULL};
        read_fork("wl-paste", argument_list, cb_buffer_ + msg_len, buffer_size - msg_len, len);
    }
    else
    {
        char *const argument_list[] = {"xclip", "-selection", "clipboard", "-t", type, "-o", NULL};
        read_fork("xclip", argument_list, cb_buffer_ + msg_len, buffer_size - msg_len, len);
    }
    if (*len + msg_len <= buffer_size)
        (*len) = (*len) + msg_len;
    else
        *len = 0;
}

void write_local_clipboard(char *buf, int len)
{
    write_bit = 1;
    int fds[2];
    pipe(fds);
    int pid = fork(), status = 0;
    if (pid == 0)
    {
        while ((dup2(fds[0], STDIN_FILENO) == -1) && (errno == EINTR))
        {
        }
        close(fds[0]);
        close(fds[1]);
        if (is_wayland)
        {
            char *const argument_list[] = {"wl-copy", NULL};
            execvp("wl-copy", argument_list);
        }
        else
        {
            uint8_t type_code = buf[0];
            if (type_code == CB_TYPE_IMAGE)
            {
                char *const argument_list[] = {"xclip", "-selection", "clipboard", "-t", IMAGEPNG, "-i", NULL};
                execvp("xclip", argument_list);
            }
            else if (type_code == CB_TYPE_TEXT)
            {
                char *const argument_list[] = {"xclip", "-selection", "clipboard", "-t", PLAINTEXT, "-i", NULL};
                execvp("xclip", argument_list);
            }
            else
            {
                fprintf(stderr, "unknown type code %u.\n", type_code);
                return;
            }
        }
        exit(1);
    }
    else
    {
        close(fds[0]);
        write(fds[1], buf + 1, len - 1);
        close(fds[1]);
        waitpid(pid, &status, 0);
    }
}

// https://stackoverflow.com/questions/8755471/x11-wait-for-and-get-clipboard-text
void clipboard_monitor_loop()
{
    is_wayland = strcmp("wayland", getenv("XDG_SESSION_TYPE")) == 0;
    if (is_wayland)
    {
        debug("current session type is wayland");
    }
    else
    {
        debug("current session type is x11");
    }

    // The following code works on wayland if Xwayland is installed. Therefore...
    // TODO: do better
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy)
    {
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

    cb_buffer_ = (char *)calloc(buffer_size, sizeof(char));
    if (!cb_buffer_)
    {
        perror("failed allocate using calloc");
        return;
    }

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
            read_local_clipboard(&cb_len);
            if (cb_len <= 0)
                continue;

            broadcast_to_known(cb_buffer_, cb_len);
        }
    }

    free(cb_buffer_);
}
