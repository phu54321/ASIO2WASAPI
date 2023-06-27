//
// Created by whyask37 on 2023-06-27.
//

#ifndef ASIO2WASAPI2_UTF8CONVERT_H
#define ASIO2WASAPI2_UTF8CONVERT_H

#include <string>

std::wstring utf8_to_wstring(const std::string &str);

std::string wstring_to_utf8(const std::wstring &str);

#endif //ASIO2WASAPI2_UTF8CONVERT_H
