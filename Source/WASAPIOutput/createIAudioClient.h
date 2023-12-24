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


#ifndef TRGKASIO_CREATEIAUDIOCLIENT_H
#define TRGKASIO_CREATEIAUDIOCLIENT_H

#include <memory>
#include <mmdeviceapi.h>
#include <Audioclient.h>
#include "../pref/UserPref.h"

const int BUFFER_SIZE_REQUEST_USEDEFAULT = -1;

enum class WASAPIMode {
    Exclusive,
    Shared
};

bool FindStreamFormat(
        const std::shared_ptr<IMMDevice> &pDevice,
        UserPrefPtr pref,
        int sampleCount,
        WASAPIMode mode,
        WAVEFORMATEXTENSIBLE *pwfxt = nullptr,
        std::shared_ptr<IAudioClient> *ppAudioClient = nullptr);

#endif //TRGKASIO_CREATEIAUDIOCLIENT_H
