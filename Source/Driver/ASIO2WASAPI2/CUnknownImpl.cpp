/*  ASIO2WASAPI2 Universal ASIO Driver

    Copyright (C) 2017 Lev Minkovsky
    Copyright (C) 2023 Hyunwoo Park (phu54321@naver.com) - modifications

    This file is part of ASIO2WASAPI2.

    ASIO2WASAPI2 is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    ASIO2WASAPI2 is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with ASIO2WASAPI2; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "ASIO2WASAPI2.h"
#include "../utils/logger.h"

CUnknown *ASIO2WASAPI2::CreateInstance(LPUNKNOWN pUnk, HRESULT *phr) {
    SPDLOG_TRACE_FUNC;
    return static_cast<CUnknown *>(new ASIO2WASAPI2(pUnk, phr));
}

STDMETHODIMP ASIO2WASAPI2::NonDelegatingQueryInterface(REFIID riid, void **ppv) {
    SPDLOG_TRACE_FUNC;
    if (riid == CLSID_ASIO2WASAPI2_DRIVER) {
        return GetInterface(this, ppv);
    }
    return CUnknown::NonDelegatingQueryInterface(riid, ppv);
}
