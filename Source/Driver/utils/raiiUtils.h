//
// Created by whyask37 on 2023-06-26.
//

#ifndef ASIO2WASAPI2_RAIIUTILS_H
#define ASIO2WASAPI2_RAIIUTILS_H

#include <memory>

template<typename T>
auto make_autorelease(T *ptr) {
    return std::shared_ptr<T>(ptr, [](T *p) {
        if (p) p->Release();
    });
}

template<typename T>
auto make_autoclose(T *h) {
    return std::shared_ptr<T>(h, [](T *h) {
        if (h) CloseHandle(h);
    });
}

struct CExitEventSetter {
    HANDLE &_hEvent;

    explicit CExitEventSetter(HANDLE &hEvent) : _hEvent(hEvent) {}

    ~CExitEventSetter() {
        SetEvent(_hEvent);
        CloseHandle(_hEvent);
        _hEvent = nullptr;
    }
};


#endif //ASIO2WASAPI2_RAIIUTILS_H

