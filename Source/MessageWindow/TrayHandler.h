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


#ifndef TRGKASIO_TRAYHANDLER_H
#define TRGKASIO_TRAYHANDLER_H

static const int trayIconID = 33412;
static const int WM_TRAYICON_MSG = WM_USER + 1143;

void createTrayIcon(HWND hWnd, HICON hIcon, const TCHAR* szTip);
void setTooltip(HWND hWnd, const TCHAR* szTip);

void removeTrayIcon(HWND hWnd);

#endif //TRGKASIO_TRAYHANDLER_H
