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
#include "linux.h"
#include "msg.h"
#include "udp_helper.h"

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
        execvp("xsel", NULL);
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

}

void clipboard_monitor_loop()
{
    int fd_kb, ctrl_down = 0;

    struct input_event event_kb;

    // could it be other event on some device?
    fd_kb = open("/dev/input/event4", O_RDONLY);
    if (fd_kb <= 0)
    {
        perror("open device error\n");
        return;
    }

    while (1)
    {
        if (read(fd_kb, &event_kb, sizeof(event_kb)) == sizeof(event_kb))
        {
            if (event_kb.type == EV_KEY)
            {
                if (event_kb.value == 0 || event_kb.value == 1)//1表示按下，0表示释放，会检测到两次
                {
                    if ((event_kb.code == KEY_LEFTCTRL || event_kb.code == KEY_RIGHTCTRL) && event_kb.value)
                    {
                        ctrl_down = 1;
                    }
                    else if ((event_kb.code == KEY_LEFTCTRL || event_kb.code == KEY_RIGHTCTRL))
                    {
                        ctrl_down = 0;
                    }

                    if ((event_kb.code == KEY_C || event_kb.code == KEY_X) && event_kb.value == 1 && ctrl_down)
                    {
                        int cb_len = 0;
                        char *cb_buf = read_local_clipboard(&cb_len);
                        char send_buf[8192] = {0};
                        int msg_len = gen_msg_clipboard_update(send_buf);
                        memcpy(send_buf + msg_len, cb_buf, cb_len);
                        udp_broadcast_to_known(send_buf, msg_len + cb_len);
                    }
                }
            }
        }
    }
    close(fd_kb);
}