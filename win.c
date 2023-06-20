#include <Windows.h>
#include <stdio.h>
#include "win.h"
#include "connection.h"

extern int is_gbk;
int write_bit = 0;

int isGBK(unsigned char* data, int len) {
    int i = 0;
    while (i < len) {
        if (data[i] <= 0x7f) {
            i++;
            continue;
        }
        else {
            if (data[i] >= 0x81 &&
                data[i] <= 0xfe &&
                data[i + 1] >= 0x40 &&
                data[i + 1] <= 0xfe &&
                data[i + 1] != 0xf7) {
                i += 2;
                continue;
            }
            else {
                return 0;
            }
        }
    }
    return 1;
}

char *GBKToUTF8(char *strAscii)
{
    int l = MultiByteToWideChar(CP_ACP, 0, strAscii, -1, NULL, 0);
    wchar_t* wstr = calloc(l + 1, sizeof(wchar_t));
    MultiByteToWideChar(CP_ACP, 0, strAscii, -1, wstr, l);
    l = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
    char* str = calloc(l + 1, 1);
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, str, l, NULL, NULL);
    free(wstr);
    return str;
}

char *UTF8ToGBK(char* strAscii)
{
    int len = MultiByteToWideChar(CP_UTF8, 0, strAscii, -1, NULL, 0);
    wchar_t* wszGBK = calloc(len + 1, sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, 0, strAscii, -1, wszGBK, len);
    len = WideCharToMultiByte(CP_ACP, 0, wszGBK, -1, NULL, 0, NULL, NULL);
    char* szGBK = calloc(len + 1, 1);
    WideCharToMultiByte(CP_ACP, 0, wszGBK, -1, szGBK, len, NULL, NULL);
    free(wszGBK);
    return szGBK;
}

char* read_local_clipboard(int* len)
{
    HWND hWnd = NULL;
    OpenClipboard(hWnd);
    char* buf;
    if (IsClipboardFormatAvailable(CF_TEXT))
    {
        HANDLE h = GetClipboardData(CF_TEXT);
        buf = (char*)GlobalLock(h);
        if (buf == 0)
        {
            perror("Global lock failed\n");
            return NULL;
        }

        char* tmp = calloc(strlen(buf) + 1, 1);
        memcpy(tmp, buf, strlen(buf));
        buf = tmp;

        GlobalUnlock(h);
        CloseClipboard();
        if (isGBK(buf, *len))
        {
            char *t = GBKToUTF8(buf);
            free(buf);
            buf = t;
        }
        *len = strlen(buf);
        return buf;
    } 
    else
    {
        return NULL;
    }
}

void write_local_clipboard(char* buf, int len)
{
    HWND hWnd = NULL;
    OpenClipboard(hWnd);
    EmptyClipboard();
    HANDLE hHandle = GlobalAlloc(GMEM_FIXED, len + 1);
    if (hHandle == 0)
    {
        perror("Global alloc failed\n");
        return;
    }
    char* pData = (char*)GlobalLock(hHandle);
    if (pData == 0)
    {
        perror("Global lock failed\n");
        return;
    }

    write_bit = 1;
    char* t = calloc(len + 1, 1);
    memcpy(t, buf, len);
    if (is_gbk)
    {
        char *tmp = UTF8ToGBK(buf);
        free(t);
        t = tmp;
    }

    strcpy(pData, t);
    SetClipboardData(CF_TEXT, hHandle);
    GlobalUnlock(hHandle);
    CloseClipboard();
    free(t);
}

LRESULT CALLBACK ClipWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static BOOL bListening = FALSE;
    switch (uMsg)
    {
    case WM_CREATE:
        bListening = AddClipboardFormatListener(hWnd);
        return bListening ? 0 : -1;

    case WM_DESTROY:
        if (bListening)
        {
            RemoveClipboardFormatListener(hWnd);
            bListening = FALSE;
        }
        return 0;

    case WM_CLIPBOARDUPDATE:
        {
            if (write_bit)
            {
                write_bit = 0;
                return 0;
            }
            int cb_len = 0;
            char* cb_buf = read_local_clipboard(&cb_len);
            char send_buf[8192] = { 0 };
            int msg_len = gen_msg_clipboard_update(send_buf);
            memcpy(send_buf + msg_len, cb_buf, cb_len);
            udp_broadcast_to_known(send_buf, msg_len + cb_len);
            return 0;
        }
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

void clipboard_monitor_loop()
{
    // https://stackoverflow.com/questions/65840288/monitor-clipboard-changes-c-for-all-applications-windows
    WNDCLASSEX wndClass = { sizeof(WNDCLASSEX) };
    wndClass.lpfnWndProc = ClipWndProc;
    wndClass.lpszClassName = L"clipboardshare";
    if (!RegisterClassEx(&wndClass))
    {
        printf("RegisterClassEx error 0x%08X\n", GetLastError());
        return 1;
    }
    HWND hWnd = CreateWindowEx(0, wndClass.lpszClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, GetModuleHandle(NULL), NULL);
    if (!hWnd)
    {
        printf("CreateWindowEx error 0x%08X\n", GetLastError()); 
        return 2;
    }

    MSG msg;
    BOOL bRet;
    while (bRet = GetMessage(&msg, 0, 0, 0)) {
        if (bRet == -1)
        {
            printf("GetMessage error 0x%08X\n", GetLastError()); return 3;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}