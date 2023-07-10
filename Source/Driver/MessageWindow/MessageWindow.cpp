#include <Windows.h>
#include <queue>
#include "MessageWindow.h"
#include "../utils/logger.h"
#include "../utils/AppException.h"
#include "../resource.h"
#include "TrayHandler.h"
#include "KeyDownListener.h"

extern HINSTANCE g_hInstDLL;

class MessageWindowImpl {
public:
    MessageWindowImpl();

    ~MessageWindowImpl();

    void setTrayTooltip(const tstring &str);

    bool popKeyDownTime(double *out);

private:
    static bool RegisterWindowClass(HINSTANCE hInstDLL, HICON hIcon);

    std::exception_ptr _threadException = nullptr;
    std::mutex _mutex;
    std::thread _thread;
    HWND _hWnd = nullptr;
    std::queue<double> _keyDownTimeQueue;

private:
    static void threadProc(HINSTANCE hInstDLL, HICON hIcon, MessageWindowImpl *p);

    static LRESULT __stdcall MessageWindowProc(
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

bool MessageWindow::popKeyDownTime(double *out) {
    return _pImpl->popKeyDownTime(out);
}

/////////////////

static const TCHAR *szWndClassName = TEXT("ASIO2WASAPI2MessageWindow");

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
            mainlog->error("Cannot register ASIO2WASAPI2MessageWindow class");
        } else {
            initialized = true;
        }
        return initialized;
    }
    return initialized;
}

MessageWindowImpl::MessageWindowImpl() {
    HINSTANCE hInstDLL = g_hInstDLL;
    HICON hIcon = LoadIcon(g_hInstDLL, MAKEINTRESOURCE(IDI_ICON1));

    if (!RegisterWindowClass(hInstDLL, hIcon)) {
        throw AppException("Cannot register ASIO2WASAPI2 Message window");
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
        throw AppException("Cannot create ASIO2WASAPI2 Message window");
    }
}


MessageWindowImpl::~MessageWindowImpl() {
    SendMessage(_hWnd, WM_CLOSE, 0, 0);
    _thread.join();
}

////////////////////////////////////////////////////////

void MessageWindowImpl::threadProc(HINSTANCE hInstDLL, HICON hIcon, MessageWindowImpl *p) {
    SPDLOG_TRACE_FUNC;

    HWND hWnd = CreateWindow(
            szWndClassName,
            TEXT("ASIO2WASAPI2 Message Handler Window"),
            0,
            0, 0, 0, 0,
            HWND_MESSAGE,
            nullptr,
            hInstDLL,
            nullptr
    );

    if (!hWnd) {
        mainlog->error("Cannot create message handler window");
        p->_threadException = std::make_exception_ptr(AppException("Cannot create ASIO2WASAPI2 Message window"));
        return;
    }
    SetWindowLongPtr(hWnd, 0, reinterpret_cast<LONG_PTR>(p));
    p->_hWnd = hWnd;

    createTrayIcon(hWnd, hIcon, TEXT("ASIO2WASAPI2"));
    initRawInput(hWnd);

    MSG msg;
    while (GetMessage(&msg, p->_hWnd, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void MessageWindowImpl::setTrayTooltip(const tstring &str) {
    std::lock_guard<std::mutex> lock(_mutex);

    setTooltip(_hWnd, str.c_str());
}

bool MessageWindowImpl::popKeyDownTime(double *out) {
    std::lock_guard<std::mutex> lock(_mutex);
    if (_keyDownTimeQueue.empty()) return false;
    *out = _keyDownTimeQueue.front();
    _keyDownTimeQueue.pop();
    return true;
}

LRESULT MessageWindowImpl::MessageWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_INPUT:
            // NOTE: according to experiment, GetMesssageTime doesn't give accurate enough timing.
            // as it is capped to 15ms accuracy... xD (It doesn't get accurate with timeBeginPeriod)
            // So as a proxy, we just believe that WndProc will fire ASAP and call accurateTime()
            // on WndProc.
            if (processRawInputMessage(hWnd, uMsg, wParam, lParam)) {
                auto time = accurateTime();
                mainlog->debug("Keyboard input on time {}", time);

                auto p = reinterpret_cast<MessageWindowImpl *>(GetWindowLongPtr(hWnd, 0));
                std::lock_guard<std::mutex> lock(p->_mutex);
                p->_keyDownTimeQueue.push(time);
            }
            return 0;
        case WM_DESTROY:
            removeTrayIcon();
            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
}
