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
    HANDLE &m_hEvent;

    explicit CExitEventSetter(HANDLE &hEvent) : m_hEvent(hEvent) {}

    ~CExitEventSetter() {
        SetEvent(m_hEvent);
        CloseHandle(m_hEvent);
        m_hEvent = nullptr;
    }
};


#endif //ASIO2WASAPI2_RAIIUTILS_H

