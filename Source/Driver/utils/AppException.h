//
// Created by whyask37 on 2023-06-29.
//

#pragma once

#ifndef ASIO2WASAPI2_APPEXCEPTION_H
#define ASIO2WASAPI2_APPEXCEPTION_H

#include <exception>
#include <string>

class AppException : public std::exception {
private:
    std::string message;

public:
    explicit AppException(std::string s) : message(std::move(s)) {}

    const char *what() const override {
        return message.c_str();
    }
};

#endif //ASIO2WASAPI2_APPEXCEPTION_H
