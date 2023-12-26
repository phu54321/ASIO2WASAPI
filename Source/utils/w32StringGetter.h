// Copyright (C) 2023 Hyunwoo Park
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
//

#ifndef TRGKASIO_W32STRINGGETTER_H
#define TRGKASIO_W32STRINGGETTER_H

#include <string>
#include <Windows.h>

std::wstring getDlgText(HWND hDlgItem, int id, UINT getLengthMsg, UINT getTextMsg);

std::wstring getWndText(HWND hWnd);

std::wstring getResourceString(HINSTANCE hInstance, UINT stringID);

#define ComboBox_GetWString(hDlgItem, id) getDlgText(hDlgItem, id, CB_GETLBTEXTLEN, CB_GETLBTEXT)
#define ListBox_GetWString(hDlgItem, id) getDlgText(hDlgItem, id, LB_GETTEXTLEN, LB_GETTEXT)

#endif //TRGKASIO_W32STRINGGETTER_H
