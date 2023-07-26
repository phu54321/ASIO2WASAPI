// Copyright (C) 2023 Hyun Woo Park
//
// This file is part of ASIO2WASAPI2.
//
// ASIO2WASAPI2 is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// ASIO2WASAPI2 is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with ASIO2WASAPI2.  If not, see <http://www.gnu.org/licenses/>.


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

