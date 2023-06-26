#include "TrayOpener.hpp"
#include "logger.h"
#include <shellapi.h>
#include <set>
#include <atomic>

const int TRAY_ICON_ID_START = 33412;
static std::atomic<int> lastTrayIconID = TRAY_ICON_ID_START;
const int WM_TRAYICON_MSG = WM_USER + 1;
const TCHAR *g_windowClassName = TEXT("TrayOpenerWindow");

static std::mutex windowClassMutex;
static std::mutex trayIDOpenerMapMutex;
static std::map<WPARAM, TrayOpener *> trayIDOpenerMap;

TrayOpener::TrayOpener(HINSTANCE hInst, HICON hIcon, std::function<void()> clickCb, const TCHAR* szTip)
    : hWnd(createWindow(hInst)),
      clickCallback(clickCb),
      trayIconID(lastTrayIconID++)
{
    Logger::info(L"TrayOpener::TrayOpener");
    {
        std::lock_guard<std::mutex> lock(trayIDOpenerMapMutex);
        trayIDOpenerMap[trayIconID] = this;
    }

    NOTIFYICONDATA nid = {0};

    nid.cbSize = sizeof(nid);
    nid.hWnd = hWnd;
    nid.uID = trayIconID;
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE | NIF_SHOWTIP;
    nid.uCallbackMessage = WM_TRAYICON_MSG;
    nid.hIcon = hIcon;
    nid.uTimeout = 1000;
    lstrcpy(nid.szTip, szTip);

    Shell_NotifyIcon(NIM_ADD, &nid);
}

TrayOpener::~TrayOpener()
{
    LOGGER_TRACE_FUNC;

    NOTIFYICONDATA nid = { 0 };

    nid.cbSize = sizeof(nid);
    nid.hWnd = NULL;
    nid.uID = trayIconID;

    Shell_NotifyIcon(NIM_DELETE, &nid);

    CloseWindow(hWnd);

    {
        std::lock_guard<std::mutex> lock(trayIDOpenerMapMutex);
        auto it = trayIDOpenerMap.find(trayIconID);
        trayIDOpenerMap.erase(it);
    }
}

HWND TrayOpener::createWindow(HINSTANCE hInst)
{
    LOGGER_TRACE_FUNC;

    createWindowClass(hInst);

    return CreateWindow(
        g_windowClassName,   // Window class
        TEXT(""),            // Window text
        WS_OVERLAPPEDWINDOW, // Window style

        // Size and position
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,

        NULL,  // Parent window
        NULL,  // Menu
        hInst, // Instance handle
        NULL);
}

void TrayOpener::createWindowClass(HINSTANCE hInst)
{
    std::lock_guard<std::mutex> guard(windowClassMutex);

    static std::set<HINSTANCE> initMap;

    auto it = initMap.find(hInst);
    if (it != initMap.end())
        return;

    LOGGER_TRACE_FUNC;

    WNDCLASS wc;

    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hInstance = hInst;
    wc.lpfnWndProc = (WNDPROC)TrayWndProc;
    wc.lpszClassName = g_windowClassName;
    wc.lpszMenuName = NULL;
    wc.style = 0;
    RegisterClass(&wc);

    initMap.insert(hInst);
}

LRESULT CALLBACK TrayOpener::TrayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_TRAYICON_MSG)
    {
        std::lock_guard<std::mutex> lock(trayIDOpenerMapMutex);
        auto it = trayIDOpenerMap.find(wParam);
        if (it != trayIDOpenerMap.end())
        {
            switch (lParam)
            {
            case WM_LBUTTONUP:
                it->second->clickCallback();
                break;
            }
        }
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}
