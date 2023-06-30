//
// Created by whyask37 on 2023-06-30.
//

#ifndef ASIO2WASAPI2_HOMEDIRFILEPATH_H
#define ASIO2WASAPI2_HOMEDIRFILEPATH_H

#include <string>
#include <windows.h>

using tstring = std::basic_string<TCHAR>;

tstring homeDirFilePath(const tstring &filename);

FILE *homeDirFOpen(const TCHAR *relPath, const TCHAR *mode);

#endif //ASIO2WASAPI2_HOMEDIRFILEPATH_H
