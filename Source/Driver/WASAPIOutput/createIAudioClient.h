//
// Created by whyask37 on 2023-06-27.
//

#ifndef ASIO2WASAPI2_CREATEIAUDIOCLIENT_H
#define ASIO2WASAPI2_CREATEIAUDIOCLIENT_H

#include <memory>
#include <mmdeviceapi.h>
#include <Audioclient.h>

std::shared_ptr<IAudioClient> createAudioClient(
        const std::shared_ptr<IMMDevice> &pDevice,
        WAVEFORMATEX *pWaveFormat);

bool FindStreamFormat(
        const std::shared_ptr<IMMDevice> &pDevice,
        int nChannels,
        int nSampleRate,
        WAVEFORMATEXTENSIBLE *pwfxt = nullptr,
        std::shared_ptr<IAudioClient> *ppAudioClient = nullptr);

#endif //ASIO2WASAPI2_CREATEIAUDIOCLIENT_H
