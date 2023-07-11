// Copyright (C) 2023 Hyunwoo Park
//
// This file is part of ASIO2WASAPI2.
//
// ASIO2WASAPI2 is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// ASIO2WASAPI2 is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with ASIO2WASAPI2.  If not, see <http://www.gnu.org/licenses/>.
//

#include "KeyDownListener.h"
#include "../utils/logger.h"

KeyDownListener::KeyDownListener() : _keyPressed(256) {
    std::fill(_keyPressed.begin(), _keyPressed.end(), false);

    // There are many pre-pressed keys:
    // Prefill keyPressed array
    pollKeyPressCount();
}

KeyDownListener::~KeyDownListener() = default;

static int normalizeKey(int vKey) {
    if (vKey == VK_HOME) return VK_NUMPAD7;
    if (vKey == VK_UP) return VK_NUMPAD8;
    if (vKey == VK_PRIOR) return VK_NUMPAD9;
    if (vKey == VK_LEFT) return VK_NUMPAD4;
    if (vKey == VK_CLEAR) return VK_NUMPAD5;
    if (vKey == VK_RIGHT) return VK_NUMPAD6;
    if (vKey == VK_END) return VK_NUMPAD1;
    if (vKey == VK_DOWN) return VK_NUMPAD2;
    if (vKey == VK_NEXT) return VK_NUMPAD3;
    if (vKey == VK_INSERT) return VK_NUMPAD0;
    return vKey;
}

static bool isValidKey(unsigned char vkCode) {
    if ('A' <= vkCode && vkCode <= 'Z') return true;
    if ('0' <= vkCode && vkCode <= '9') return true;
    switch (vkCode) {
        case VK_LMENU:
        case VK_RMENU:
        case VK_LCONTROL:
        case VK_RCONTROL:
        case VK_HANGEUL:
        case VK_LSHIFT:
        case VK_RSHIFT:
        case VK_CAPITAL:
        case VK_HANJA:
        case VK_SPACE:
        case VK_APPS:

        case VK_OEM_1:
        case VK_OEM_PLUS:
        case VK_OEM_COMMA:
        case VK_OEM_MINUS:
        case VK_OEM_PERIOD:
        case VK_OEM_2:
        case VK_OEM_3:
        case VK_OEM_4:
        case VK_OEM_5:
        case VK_OEM_6:
        case VK_OEM_7:
        case VK_OEM_8:

        case VK_NUMPAD0:
        case VK_NUMPAD1:
        case VK_NUMPAD2:
        case VK_NUMPAD3:
        case VK_NUMPAD4:
        case VK_NUMPAD5:
        case VK_NUMPAD6:
        case VK_NUMPAD7:
        case VK_NUMPAD8:
        case VK_NUMPAD9:
        case VK_MULTIPLY:
        case VK_ADD:
        case VK_SEPARATOR:
        case VK_SUBTRACT:
        case VK_DIVIDE:
        case VK_DECIMAL:
            return true;

        default:
            return false;
    }
}


int KeyDownListener::pollKeyPressCount() {
    SPDLOG_TRACE_FUNC;

    int count = 0;
    for (int vKey = 0; vKey < 256; vKey++) {
        auto normalizedVK = normalizeKey(vKey);
        if (!isValidKey(normalizedVK)) continue;

        int state = GetAsyncKeyState(vKey) & 0x8000;
        if (state) {
            if (!_keyPressed[vKey]) {
                _keyPressed[vKey] = true;
                count++;
            }
        } else {
            _keyPressed[vKey] = false;
        }
    }
    return count;
}
