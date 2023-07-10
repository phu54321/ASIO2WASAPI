//
// Created by whyask37 on 2023-07-10.
//

#include "KeyDownListener.h"
#include "../utils/accurateTime.h"
#include "../utils/logger.h"
#include <hidusage.h>

void initRawInput(HWND hWnd) {
    RAWINPUTDEVICE rid[1];
    rid[0].usUsagePage = HID_USAGE_PAGE_GENERIC;
    rid[0].usUsage = HID_USAGE_GENERIC_KEYBOARD;
    rid[0].dwFlags = RIDEV_INPUTSINK;
    rid[0].hwndTarget = hWnd;
    RegisterRawInputDevices(rid, 1, sizeof(rid[0]));
}

enum KeyDirection {
    Up,
    Down
};

bool processRawInputMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    // The official Microsoft examples are pretty terrible about this.
    // Size needs to be non-constant because GetRawInputData() can return the
    // size necessary for the RAWINPUT data, which is a weird feature.
    unsigned size = sizeof(RAWINPUT);
    static RAWINPUT raw[sizeof(RAWINPUT)];
    GetRawInputData((HRAWINPUT) lParam, RID_INPUT, raw, &size, sizeof(RAWINPUTHEADER));

    if (raw->header.dwType == RIM_TYPEKEYBOARD) {
        auto &rawData = raw->data.keyboard;
        if ((rawData.Flags & RI_KEY_BREAK) == RI_KEY_MAKE) {
            return true;
        }
    }
    return false;
}
