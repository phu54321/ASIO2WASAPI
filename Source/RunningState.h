/*  ASIO2WASAPI2 Universal ASIO Driver

    Copyright (C) 2017 Lev Minkovsky
    Copyright (C) 2023 Hyunwoo Park (phu54321@naver.com) - modifications

    This file is part of ASIO2WASAPI2.

    ASIO2WASAPI2 is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    ASIO2WASAPI2 is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with ASIO2WASAPI2; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/


#ifndef ASIO2WASAPI2_RUNNINGSTATE_H
#define ASIO2WASAPI2_RUNNINGSTATE_H

#include <Windows.h>
#include "PreparedState.h"
#include "MessageWindow/MessageWindow.h"
#include "WASAPIOutput/ClapRenderer.h"
#include "MessageWindow/KeyDownListener.h"
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "tracy/Tracy.hpp"

class RunningState {
public:
    explicit RunningState(PreparedState *p);

    ~RunningState();

    void signalOutputReady();

private:
    void signalStop();

    bool _throttle;

    PreparedState *_preparedState;
    bool _isOutputReady = true;
    bool _pollStop = false;

    TracyLockable(std::mutex, _mutex);
    std::condition_variable_any _notifier;
    std::thread _pollThread;

    std::vector<WASAPIOutputPtr> _outputList;
    ClapRenderer _clapRenderer;
    MessageWindow _msgWindow;
    KeyDownListener _keyListener;

    static void threadProc(RunningState *state);
};

#endif //ASIO2WASAPI2_RUNNINGSTATE_H
