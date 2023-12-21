// Copyright (C) 2023 Hyunwoo Park
//
// This file is part of ASIO2WASAPI2.
//
// ASIO2WASAPI2 is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// ASIO2WASAPI2 is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with ASIO2WASAPI2.  If not, see <http://www.gnu.org/licenses/>.
//

#include <nana/gui.hpp>
#include <nana/gui/widgets/label.hpp>
#include <nana/gui/widgets/button.hpp>
#include <thread>
#include "../utils/dpiRaii.h"

void driverSettingsGUIThread() {
    using namespace nana;
    DpiRAII raii;

    //Define a form.
    form fm;

    //Define a label and display a text.
    label lab{fm, "Hello, <bold blue size=16>Nana C++ Library</>"};
    lab.format(true);

    //Define a button and answer the click event.
    button btn{fm, "Quit"};
    btn.events().click([&fm] {
        fm.close();
    });

    //Layout management
    fm.div("vert <><<><weight=80% text><>><><weight=24<><button><>><>");
    fm["text"] << lab;
    fm["button"] << btn;
    fm.collocate();

    //Show the form
    fm.show();

    //Start to event loop process, it blocks until the form is closed.
    exec();
}

#ifdef ASIO2WASAPI_PREFGUI_TEST_MAIN
int main() {
    std::thread t(driverSettingsGUIThread);
    t.join();
}

#endif