// Copyright (C) 2023 Hyunwoo Park
//
// This file is part of trgkASIO.
//
// trgkASIO is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// trgkASIO is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with trgkASIO.  If not, see <http://www.gnu.org/licenses/>.
//

#include <Windows.h>
#include <queue>
#include <set>
#include <shellapi.h>
#include "MessageWindow.h"
#include "../utils/logger.h"
#include "../utils/AppException.h"
#include "../res/resource.h"
#include "TrayHandler.h"
#include "KeyDownListener.h"
#include <tracy/Tracy.hpp>
#include "../version.h"
#include "../utils/ResourceLoad.h"
#include "../pref/UserPrefGUI.h"
#include "../utils/w32StringGetter.h"

extern HINSTANCE g_hInstDLL;

class MessageWindowImpl {
public:
    MessageWindowImpl();

    ~MessageWindowImpl();

    void setTrayTooltip(const tstring &str);

private:
    static bool RegisterWindowClass(HINSTANCE hInstDLL, HICON hIcon);

    std::exception_ptr _threadException = nullptr;
    TracyLockable(std::mutex, _mutex);
    std::thread _thread;
    HWND _hWnd = nullptr;
    std::set<HWND> _hDlgSet;

private:
    static void threadProc(HINSTANCE hInstDLL, HICON hIcon, MessageWindowImpl *p);

    static LRESULT __stdcall MessageWindowProc(
            HWND hWnd,
            UINT uMsg,
            WPARAM wParam,
            LPARAM lParam
    );

    static INT_PTR __stdcall AboutDialogProc(
            HWND hWnd,
            UINT uMsg,
            WPARAM wParam,
            LPARAM lParam
    );


};

MessageWindow::MessageWindow()
        : _pImpl(std::make_unique<MessageWindowImpl>()) {}

MessageWindow::~MessageWindow() = default;

void MessageWindow::setTrayTooltip(const tstring &msg) {
    _pImpl->setTrayTooltip(msg);
}

/////////////////

static const TCHAR *szWndClassName = TEXT("TrgkASIOMessageWindow");

bool MessageWindowImpl::RegisterWindowClass(HINSTANCE hInstDLL, HICON hIcon) {
    static std::recursive_mutex _staticMutex;
    std::lock_guard<std::recursive_mutex> lock(_staticMutex);

    static bool registered = false;
    static bool initialized = false;

    if (!registered) {
        registered = true;
        WNDCLASS wc = {0};
        wc.lpfnWndProc = (WNDPROC) MessageWindowProc;
        wc.cbClsExtra = 0;
        wc.cbWndExtra = sizeof(MessageWindowImpl *);
        wc.hInstance = hInstDLL;
        wc.hIcon = hIcon;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH) (COLOR_WINDOW + 1);
        wc.lpszMenuName = nullptr;
        wc.lpszClassName = szWndClassName;

        if (!RegisterClass(&wc)) {
            mainlog->error("Cannot register TrgkASIOMessageWindow class");
        } else {
        }
        initialized = true;
        return initialized;
    }
    return initialized;
}

MessageWindowImpl::MessageWindowImpl() {
    HINSTANCE hInstDLL = g_hInstDLL;
    HICON hIcon = LoadIcon(g_hInstDLL, MAKEINTRESOURCE(IDI_MAINICON));

    if (!RegisterWindowClass(hInstDLL, hIcon)) {
        throw AppException("Cannot register TrgkASIO Message window");
    }

    // Create main thread
    _thread = std::thread(threadProc, hInstDLL, hIcon, this);

    // HWND should be created on the thread GetMessage is called, so
    // we should wait until threadProc fills in HWND
    const DWORD timeout = 3000;
    DWORD timeStart = GetTickCount();
    // Since GetTickCount() wraps around (2<<32), we cannot write conditional like
    // GetTickCount() < timeStart + timeout
    while (!_hWnd && (GetTickCount() - timeStart) < timeout) {
        // Just wait...
        Sleep(1);
        if (_threadException) {
            _thread.join();
            std::rethrow_exception(_threadException);
        }
    }
    if (!_hWnd) {
        throw AppException("Cannot create TrgkASIO Message window");
    }
}


MessageWindowImpl::~MessageWindowImpl() {
    SendMessage(_hWnd, WM_CLOSE, 0, 0);
    _thread.join();
}

////////////////////////////////////////////////////////

void MessageWindowImpl::threadProc(HINSTANCE hInstDLL, HICON hIcon, MessageWindowImpl *p) {
    ZoneScoped;

    HWND hWnd = CreateWindow(
            szWndClassName,
            TEXT("TrgkASIO Message Handler Window"),
            0,
            0, 0, 0, 0,
            HWND_MESSAGE,
            nullptr,
            hInstDLL,
            nullptr
    );

    if (!hWnd) {
        mainlog->error("Cannot create message handler window");
        p->_threadException = std::make_exception_ptr(AppException("Cannot create TrgkASIO Message window"));
        return;
    }
    SetWindowLongPtr(hWnd, 0, reinterpret_cast<LONG_PTR>(p));
    p->_hWnd = hWnd;

    createTrayIcon(hWnd, hIcon, TEXT("TrgkASIO"));

    MSG msg;
    BOOL bret;
    while ((bret = GetMessage(&msg, nullptr, 0, 0)) != 0) {
        if (bret == -1) {
            mainlog->error("GetMessage returned 0x{:08X}", bret);
            break;
        }
        bool dlgMsgProcessed = false;
        for (auto &dlg: p->_hDlgSet) {
            if (IsDialogMessage(dlg, &msg)) {
                dlgMsgProcessed = true;
                break;
            }
        }
        if (dlgMsgProcessed) continue;

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void MessageWindowImpl::setTrayTooltip(const tstring &str) {
    std::lock_guard lock(_mutex);

    setTooltip(_hWnd, str.c_str());
}

const int MENU_PREFERENCE = 0x101;
const int MENU_ABOUT = 0x103;


LRESULT MessageWindowImpl::MessageWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_DESTROY:
            removeTrayIcon(hWnd);
            PostQuitMessage(0);
            return 0;

        case WM_TRAYICON_MSG: {
            if (wParam == trayIconID) {
                if (lParam == WM_LBUTTONUP || lParam == WM_RBUTTONUP) {
                    auto hInstance = (HINSTANCE) GetWindowLongPtr(hWnd, GWLP_HINSTANCE);
                    auto hMenu = CreateMenu();

                    AppendMenuW(hMenu, MF_STRING, MENU_ABOUT, getResourceString(hInstance, IDS_ABOUT).c_str());
                    AppendMenuW(hMenu, MF_STRING, MENU_PREFERENCE,
                                getResourceString(hInstance, IDS_PREFERENCE).c_str());

                    auto hMenubar = CreateMenu();
                    AppendMenu(hMenubar, MF_POPUP, (UINT_PTR) hMenu, getResourceString(hInstance, IDS_MENU).c_str());

                    auto hPopupMenu = GetSubMenu(hMenubar, 0);
                    POINT pt;
                    GetCursorPos(&pt);
                    SetForegroundWindow(hWnd);
                    TrackPopupMenu(hPopupMenu, TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd,
                                   NULL);
                    SetForegroundWindow(hWnd);
                    PostMessage(hWnd, WM_NULL, 0, 0);

                    DestroyMenu(hMenu);
                    DestroyMenu(hMenubar);
                    return 0;
                }
            }
            break;
        }

        case WM_COMMAND: {
            auto p = reinterpret_cast<MessageWindowImpl *>(GetWindowLongPtr(hWnd, 0));
            auto wmId = LOWORD(wParam);
            if (wmId == MENU_ABOUT) {
                auto hDlg = CreateDialog(g_hInstDLL, MAKEINTRESOURCE(IDD_ABOUT), hWnd, AboutDialogProc);
                ShowWindow(hDlg, SW_SHOW);
                p->_hDlgSet.insert(hDlg);
                return 0;
            } else if (wmId == MENU_PREFERENCE) {
                auto hDlg = createUserPrefEditDialog(g_hInstDLL, hWnd);
                ShowWindow(hDlg, SW_SHOW);
                p->_hDlgSet.insert(hDlg);
                return 0;
            }
            break;
        }

        case WM_USER_REMOVEDLG: {
            auto p = reinterpret_cast<MessageWindowImpl *>(GetWindowLongPtr(hWnd, 0));
            auto &dlgSet = p->_hDlgSet;
            auto it = dlgSet.find((HWND) lParam);
            if (it != dlgSet.end()) {
                dlgSet.erase(it);
            }
            return 0;
        }

        default:;
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

INT_PTR MessageWindowImpl::AboutDialogProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_INITDIALOG: {
            auto licenseTextBuffer = loadUserdataResource(g_hInstDLL, MAKEINTRESOURCE(IDR_LICENSE_TEXT));
            std::string licenseTextString{licenseTextBuffer.begin(), licenseTextBuffer.end()};
            SetDlgItemText(hWnd, IDC_VERSION_TEXT, TEXT("TrgkASIO ") SPRODUCT_VERSION);
            SetDlgItemTextA(hWnd, IDC_LICENSE_TEXT, licenseTextString.c_str());
            return TRUE;
        }

        case WM_CLOSE: {
            auto parentWindow = (HWND) GetWindowLongPtr(hWnd, GWLP_HWNDPARENT);
            if (parentWindow) {
                SendMessage(parentWindow, WM_USER_REMOVEDLG, 0, (LPARAM) hWnd);
            }
            DestroyWindow(hWnd);
            return TRUE;
        }

        case WM_COMMAND: {
            WORD id = LOWORD(wParam);
            if (id == IDB_GITHUB) {
                ShellExecute(
                        nullptr, nullptr,
                        L"https://github.com/phu54321/TrgkASIO",
                        nullptr, nullptr, SW_SHOW);
            }
            return TRUE;
        }

        default:
            return FALSE;
    }
    return TRUE;
}
