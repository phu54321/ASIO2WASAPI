//
// Created by whyask37 on 2023-06-30.
//

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
