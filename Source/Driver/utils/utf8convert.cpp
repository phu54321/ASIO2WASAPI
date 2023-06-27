//
// Created by whyask37 on 2023-06-27.
//

#include "utf8convert.h"
#include <codecvt>
#include <locale>

// convert UTF-8 string to std::wstring
std::wstring utf8_to_wstring(const std::string &str) {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;
    return myconv.from_bytes(str);
}

// convert std::wstring to UTF-8 string
std::string wstring_to_utf8(const std::wstring &str) {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;
    return myconv.to_bytes(str);
}