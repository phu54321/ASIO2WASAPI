//
// Created by whyask37 on 2023-07-10.
//

#ifndef ASIO2WASAPI2_KEYDOWNLISTENER_H
#define ASIO2WASAPI2_KEYDOWNLISTENER_H

#include <Windows.h>

void initRawInput(HWND hWnd);

bool processRawInputMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

#endif //ASIO2WASAPI2_KEYDOWNLISTENER_H
