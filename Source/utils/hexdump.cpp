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


#include "hexdump.h"
#include <vector>
#include <cstdint>

std::string hexdump(const void* p, int size) {
    static const char* chTable = "0123456789abcdef";
    std::vector<char> buffer(size * 3);
    auto b  = reinterpret_cast<const uint8_t*>(p);

    for (int i = 0 ; i < size ; i++) {
        buffer[i * 3 + 0] = chTable[b[i] >> 4];
        buffer[i * 3 + 1] = chTable[b[i] & 0xf];
        buffer[i * 3 + 2] = ' ';
    }
    return { buffer.begin(), buffer.end() };
}
