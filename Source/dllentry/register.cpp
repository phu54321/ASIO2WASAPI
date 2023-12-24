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

#include "../lib/WinReg.hpp"
#include "../TrgkASIO.h"
#include "../utils/homeDirFilePath.h"
#include "../utils/AppException.h"
#include <Windows.h>
#include <tchar.h>
#include <spdlog/fmt/fmt.h>
#include <spdlog/fmt/xchar.h>

using namespace winreg;

#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"

const int DEV_ERR_SELFREG = -100;
const int ERRSREG_MODULE_NOT_FOUND = DEV_ERR_SELFREG - 1;
const int ERRSREG_MODPATH_NOT_FOUND = DEV_ERR_SELFREG - 2;
const int ERRSREG_STRING_FROM_CLSID = DEV_ERR_SELFREG - 3;
const int ERRSREG_CHAR_TO_MULTIBYTE = DEV_ERR_SELFREG - 4;

extern HINSTANCE g_hInstDLL;

// Convert CLSID_TRGKASIO_DRIVER to tstring
static tstring GetCLSIDString() {
    tstring clsIdString;
    LPOLESTR oClsId = nullptr;
    LONG rc = (LONG) StringFromCLSID(CLSID_TRGKASIO_DRIVER, &oClsId);
    if (rc != S_OK) {
        throw AppException("StringFromCLSID failed!");
    }
    clsIdString = oClsId;
    CoTaskMemFree(oClsId);
    return clsIdString;
}

HRESULT DllRegisterServer() {
    OutputDebugString(TEXT("DLLRegisterServer"));

    auto clsIdString = GetCLSIDString();

    // Get current dll path
    tstring dllPath;
    {
        TCHAR szDllPathName[MAX_PATH] = {0};
        GetModuleFileName(g_hInstDLL, szDllPathName, MAX_PATH);
        dllPath = szDllPathName;
    }

    //
    {
        auto keyRoot = fmt::format(TEXT("CLSID\\{}"), clsIdString);
        auto msg = fmt::format(TEXT("Touching HKCR\\{}"), keyRoot);
        OutputDebugString(msg.c_str());

        RegKey key{HKEY_CLASSES_ROOT, keyRoot};
        key.SetStringValue(TEXT("Description"), TEXT("trgkASIO"));
    }

    {
        auto keyRoot = fmt::format(TEXT("CLSID\\{}\\InProcServer32"), clsIdString);
        auto msg = fmt::format(TEXT("Touching HKCR\\{}"), keyRoot);
        OutputDebugString(msg.c_str());

        RegKey key{HKEY_CLASSES_ROOT, keyRoot};
        key.SetStringValue(TEXT(""), dllPath);
        key.SetStringValue(TEXT("ThreadingModel"), TEXT("Apartment"));
    }

    {
        auto keyRoot = TEXT("SOFTWARE\\ASIO\\trgkASIO");
        auto msg = fmt::format(TEXT("Touching HKLM\\{}"), keyRoot);
        OutputDebugString(msg.c_str());

        RegKey key{HKEY_LOCAL_MACHINE, keyRoot};
        key.SetStringValue(TEXT("CLSID"), clsIdString);
        key.SetStringValue(TEXT("Description"), TEXT("trgkASIO"));
    }

    OutputDebugString(TEXT("Done!"));
    return S_OK;
}

HRESULT DllUnregisterServer() {
    // Convert CLSID_TRGKASIO_DRIVER to tstring
    auto clsIdString = GetCLSIDString();

    //
    {
        RegKey key{HKEY_CLASSES_ROOT, TEXT("CLSID")};
        key.DeleteTree(clsIdString);
    }

    {
        RegKey key{HKEY_LOCAL_MACHINE, TEXT("SOFTWARE\\ASIO")};
        key.DeleteTree(TEXT("trgkASIO"));
    }

    return S_OK;
}

#pragma clang diagnostic pop
