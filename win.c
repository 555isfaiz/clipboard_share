#include <Windows.h>
#include <stdio.h>
#include "win.h"
#include "udp_helper.h"

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
        GlobalUnlock(h);
        CloseClipboard();
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
    strcpy(pData, buf);
    SetClipboardData(CF_TEXT, hHandle);
    GlobalUnlock(hHandle);
    CloseClipboard();
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