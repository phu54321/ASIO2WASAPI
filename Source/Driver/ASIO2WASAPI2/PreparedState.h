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


#ifndef ASIO2WASAPI2_PREPAREDSTATE_H
#define ASIO2WASAPI2_PREPAREDSTATE_H

#include <memory>
#include <vector>
#include "asiosys.h"
#include "asio.h"
#include "../WASAPIOutput/WASAPIOutput.h"
#include "ASIO2WASAPI2Impl.h"

class RunningState;

class PreparedState {
    friend class RunningState;

public:
    PreparedState(const std::vector<IMMDevicePtr> &pDeviceList, DriverSettings settings, ASIOCallbacks *callbacks);

    ~PreparedState();

    void InitASIOBufferInfo(ASIOBufferInfo *infos, int infoCount);

    bool start();

    bool stop();

public:
    void outputReady();

    void requestReset();

    ASIOError getSamplePosition(ASIOSamples *sPos, ASIOTimeStamp *tStamp) const;

    int getBufferSize() const { return _bufferSize; }

private:

    DriverSettings _settings;
    ASIOCallbacks *_callbacks;
    std::vector<IMMDevicePtr> _pDeviceList;

    int _bufferSize = 0; // in audio frames
    int _bufferIndex = 0;
    std::vector<std::vector<short>> _buffers[2];

    ASIOTimeStamp _theSystemTime = {0, 0};
    uint64_t _samplePosition = 0;
    std::shared_ptr<RunningState> _runningState;

};


#endif //ASIO2WASAPI2_PREPAREDSTATE_H
