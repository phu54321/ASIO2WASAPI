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

#include "KeyDownListener.h"
#include "../utils/logger.h"
#include <Windows.h>
#include <vector>

KeyDownListener::KeyDownListener(bool cpuThrottle) : _thread(threadProc, this), _cpuThrottle(cpuThrottle) {}

KeyDownListener::~KeyDownListener() {
    _killThread = true;
    _thread.join();
}

KeyEventCount KeyDownListener::pollKeyEventCount() {
    KeyEventCount kc = {0};
    kc.keyDown = _keyDownCount.exchange(0);
    kc.keyUp = _keyUpCount.exchange(0);
    return kc;
}

////


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
        case VK_LWIN:
        case VK_RWIN:
        case VK_NUMLOCK:
        case VK_SCROLL:
        case VK_CAPITAL:
        case VK_HANJA:
        case VK_SPACE:
        case VK_APPS:
        case VK_RETURN:
        case VK_ESCAPE:
        case VK_TAB:

        case VK_F1:
        case VK_F2:
        case VK_F3:
        case VK_F4:
        case VK_F5:
        case VK_F6:
        case VK_F7:
        case VK_F8:
        case VK_F9:
        case VK_F10:
        case VK_F11:
        case VK_F12:
        case VK_PAUSE:
        case VK_PRINT:
        case VK_DELETE:
        case VK_BACK:

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


void KeyDownListener::threadProc(KeyDownListener *p) {
    bool _keyPressed[256] = {false};
    bool initialRun = true;

    while (!p->_killThread) {
//        mainlog->debug("KeyDownListener threadProc loop");

        /// Rationale of using GetAsyncKeyState
        /// 1. Raw Input API - Games frequently utilizes them, and getting multiple
        ///   raw input api concurrently on a game process is quite hard to get reliably.
        /// 2. Keybaord Hooks - Raw Input API interferes with WH_KEYBOARD_LL things when
        ///   the hook were held in the same process, so while dll can capture keystroke from
        ///   any other application, it cannot get one from itself.
        /// 3. GetKeyboardState - Interferes with message queue of the thread. This only works if
        ///   WM_KEYDOWN like messages are sent to the current message queue. This also interferes
        ///   with raw input API
        /// So while seemingly not-so-efficient and slow, looping all keys through GetAsyncKeyState
        /// is the most reliable way to get keyboard states
        for (int vKey = 0; vKey < 256; vKey++) {
            auto normalizedVK = normalizeKey(vKey);
            if (!isValidKey(normalizedVK)) continue;

            int state = GetAsyncKeyState(vKey) & 0x8000;
            if (state) {
                if (!_keyPressed[vKey]) {
                    _keyPressed[vKey] = true;
                    if (!initialRun) p->_keyDownCount++;
                }
            } else {
                if (_keyPressed[vKey]) {
                    _keyPressed[vKey] = false;
                    if (!initialRun) p->_keyUpCount++;
                }
            }
        }
        initialRun = false;
        if (p->_cpuThrottle) Sleep(1);
        else Sleep(0);
    }
}
