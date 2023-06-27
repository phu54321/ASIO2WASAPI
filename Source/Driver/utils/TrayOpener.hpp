#pragma once

#include <Windows.h>
#include <mutex>
#include <functional>
#include <map>

class TrayOpener {
public:
    TrayOpener(HINSTANCE hInstance, HICON hIcon, std::function<void()> clickCb, const TCHAR *szTip);

    ~TrayOpener();

private:
    HWND hWnd;
    std::function<void()> clickCallback;
    int trayIconID;

private:
    static void createWindowClass(HINSTANCE hInstance);

    static HWND createWindow(HINSTANCE hInst);

    static LRESULT CALLBACK TrayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
};
