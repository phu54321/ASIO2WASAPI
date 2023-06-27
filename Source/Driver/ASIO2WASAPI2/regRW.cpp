//
// Created by whyask37 on 2023-06-27.
//

#include "ASIO2WASAPI2.h"
#include "../utils/logger.h"
#include "../utils/json.hpp"
#include "../utils/utf8convert.h"

using json = nlohmann::json;

const TCHAR *szJsonRegValName = TEXT("json");
const TCHAR *szPrefsRegKey = TEXT("Software\\ASIO2WASAPI2");

void ASIO2WASAPI2::readFromRegistry() {
    LOGGER_TRACE_FUNC;
    Logger::debug(L"ASIO2WASAPI2::readFromRegistery");
    HKEY key = 0;
    LONG lResult = RegOpenKeyEx(HKEY_CURRENT_USER, szPrefsRegKey, 0, KEY_READ, &key);
    if (ERROR_SUCCESS == lResult) {
        DWORD size;

        RegGetValue(key, NULL, szJsonRegValName, RRF_RT_REG_SZ, NULL, NULL, &size);
        if (size) {
            std::vector<BYTE> v(size);
            RegGetValue(key, NULL, szJsonRegValName, RRF_RT_REG_SZ, NULL, v.data(), &size);
            try {
                json j = json::parse(v.begin(), v.end());
                int nSampleRate = j["nSampleRate"];
                int nChannels = j["nChannels"];
                std::wstring deviceId = utf8_to_wstring(j["deviceId"]);

                m_nSampleRate = nSampleRate;
                m_nChannels = nChannels;
                m_deviceId = deviceId;
                Logger::debug(L" - m_nChannels: %d", m_nChannels);
                Logger::debug(L" - m_nSampleRate: %d", m_nSampleRate);
                Logger::debug(L" - m_deviceId: %ws", m_deviceId.c_str());
            }
            catch (json::exception &e) {
                Logger::error(L"JSON error: %s", e.what());
            }
        }
        RegCloseKey(key);
    }
}

void ASIO2WASAPI2::writeToRegistry() {
    LOGGER_TRACE_FUNC;
    HKEY key = 0;
    LONG lResult = RegCreateKeyEx(HKEY_CURRENT_USER, szPrefsRegKey, 0, NULL, 0, KEY_WRITE, NULL, &key, NULL);
    if (ERROR_SUCCESS == lResult) {
        json j = {
                {"nChannels",   m_nChannels},
                {"nSampleRate", m_nSampleRate},
                {"deviceId",    wstring_to_utf8(m_deviceId)}};
        auto jsonString = j.dump();
        RegSetValueEx(key, szJsonRegValName, NULL, REG_SZ, (const BYTE *) jsonString.data(), (DWORD) jsonString.size());
        RegCloseKey(key);
        Logger::debug(L" - m_nChannels: %d", m_nChannels);
        Logger::debug(L" - m_nSampleRate: %d", m_nSampleRate);
        Logger::debug(L" - m_deviceId: %ws", &m_deviceId[0]);
    }
}
