#pragma once

#include <Windows.h>
#include <mutex>
#include <functional>
#include <map>
#include <memory>
#include "../utils/homeDirFilePath.h"

class MessageWindowImpl;

class MessageWindow {
public:
    MessageWindow();
    ~MessageWindow();

    void setTrayTooltip(const tstring &msg);

    bool popKeyDownTime(double *out);

private:
    std::unique_ptr<MessageWindowImpl> _pImpl;
};
