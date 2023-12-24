// Copyright (C) 2023 Hyun Woo Park
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



#include "homeDirFilePath.h"
#include <string>
#include <ShlObj.h>

#ifndef tfopen
#ifndef UNICODE
#define tfopen fopen
#else
#define tfopen _wfopen
#endif
#endif


static tstring getHomeDir() {
    TCHAR path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPath(nullptr, CSIDL_PROFILE, nullptr, 0, path))) {
        return path;
    }
    return TEXT("");
}

static void AddTrailingSeparator(tstring &str) {
    if (str.length() == 0 || str[str.length() - 1] != '\\') {
        str += TEXT("\\");
    }
}


tstring homeDirFilePath(const tstring &filename) {
    auto homeDir = getHomeDir();
    AddTrailingSeparator(homeDir);
    return homeDir + filename;
}

FILE *homeDirFOpen(const TCHAR *relPath, const TCHAR *mode) {
    return tfopen(homeDirFilePath(relPath).c_str(), mode);
}
