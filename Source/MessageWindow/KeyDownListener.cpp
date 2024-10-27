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
#include <mmsystem.h>

static int normalizeKey(int vKey);

static bool isValidKey(unsigned char vkCode);

KeyDownListener::KeyDownListener() {
    TIMECAPS tcaps;
    timeGetDevCaps(&tcaps, sizeof(tcaps));
    _wPeriodMin = tcaps.wPeriodMin;
    timeBeginPeriod(_wPeriodMin);

    // Incur 1000hz polling rate
    _timerID = timeSetEvent(1, _wPeriodMin, _tickNotifierCallback, reinterpret_cast<DWORD_PTR>(this),
                            TIME_PERIODIC | TIME_KILL_SYNCHRONOUS);
}

KeyDownListener::~KeyDownListener() {
    timeKillEvent(_timerID);
    timeEndPeriod(_wPeriodMin);
}

KeyEventCount KeyDownListener::pollKeyEventCount() {
    std::lock_guard lock{_keyCountMutex};

    KeyEventCount kc = {0};
    kc.keyDown = _keyDownCount;
    kc.keyUp = _keyUpCount;

    _keyDownCount = 0;
    _keyUpCount = 0;
    return kc;
}

////


void CALLBACK KeyDownListener::_tickNotifierCallback(UINT uTimerID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1,
                                                     DWORD_PTR dw2) {
    auto listener = reinterpret_cast<KeyDownListener *>(dwUser);
    // In case of CPU overuse, multiple _tickNotifierCallback calls can overlap.
    // Gracefully terminate overlapping calls
    if (!listener->_tickMutex.try_lock()) return;

    std::lock_guard guard{listener->_tickMutex, std::adopt_lock};
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
    auto initialRun = listener->_initialRun;
    auto newKeyDownCount = 0;
    auto newKeyUpCount = 0;

    for (int vKey = 0; vKey < 256; vKey++) {
        auto normalizedVK = normalizeKey(vKey);
        if (!isValidKey(normalizedVK)) continue;

        int state = GetAsyncKeyState(vKey) & 0x8000;
        if (state) {
            if (!listener->_keyPressed[vKey]) {
                listener->_keyPressed[vKey] = true;
                if (!initialRun) newKeyDownCount++;
            }
        } else {
            if (listener->_keyPressed[vKey]) {
                listener->_keyPressed[vKey] = false;
                if (!initialRun) newKeyUpCount++;
            }
        }
    }

    {
        std::lock_guard keyCountLock{listener->_keyCountMutex};
        listener->_keyUpCount += newKeyUpCount;
        listener->_keyDownCount += newKeyDownCount;
    }
    listener->_initialRun = false;
}

//////


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

