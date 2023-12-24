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


#pragma once

#ifndef TRGKASIO_APPEXCEPTION_H
#define TRGKASIO_APPEXCEPTION_H

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

#endif //TRGKASIO_APPEXCEPTION_H
