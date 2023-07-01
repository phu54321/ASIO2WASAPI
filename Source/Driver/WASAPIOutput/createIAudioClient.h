//
// Created by whyask37 on 2023-06-27.
//

#ifndef ASIO2WASAPI2_CREATEIAUDIOCLIENT_H
#define ASIO2WASAPI2_CREATEIAUDIOCLIENT_H

#include <memory>
#include <mmdeviceapi.h>
#include <Audioclient.h>

const int BUFFER_SIZE_REQUEST_USEDEFAULT = -1;

enum class WASAPIMode {
    Pull,
    Event
};

bool FindStreamFormat(
        const std::shared_ptr<IMMDevice> &pDevice,
        int nChannels,
        int nSampleRate,
        int bufferSizeRequest = BUFFER_SIZE_REQUEST_USEDEFAULT,
        WASAPIMode mode = WASAPIMode::Event,
        WAVEFORMATEXTENSIBLE *pwfxt = nullptr,
        std::shared_ptr<IAudioClient> *ppAudioClient = nullptr);

#endif //ASIO2WASAPI2_CREATEIAUDIOCLIENT_H
