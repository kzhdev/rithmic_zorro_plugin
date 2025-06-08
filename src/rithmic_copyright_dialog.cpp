// The MIT License (MIT)
// Copyright (c) 2024-2025 Kun Zhao
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "stdafx.h"
#include "rithmic_copyright_dialog.h"
#include <windows.h>
#include <resource.h>
#include "global.h"
#include <string>
#include <vector>
#include <thread>

using namespace zorro;

#define IDT_AUTOCLOSE_TIMER 1

extern HINSTANCE g_hThisModule;

namespace {
    HBITMAP LoadBitmapFromDll(HINSTANCE hInst, int resourceId)
    {
        return (HBITMAP)LoadImageW(hInst,
                                    MAKEINTRESOURCEW(resourceId),
                                    IMAGE_BITMAP,
                                    0, 0,
                                    LR_DEFAULTSIZE);
    }

    INT_PTR CALLBACK CopyrightDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        static HBITMAP hRithmicBmp = NULL;
        static HBITMAP hOmneBmp = NULL;

        switch (message)
        {
            case WM_INITDIALOG:
            {
                // Set timer to close after 5000ms (3 seconds)
                SetTimer(hwnd, IDT_AUTOCLOSE_TIMER, 3000, nullptr);

                // remove title bar, system menu, thick frame
                LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
                style &= ~(WS_CAPTION | WS_SYSMENU | WS_THICKFRAME);
                SetWindowLongPtr(hwnd, GWL_STYLE, style);

                // tell Windows to recalculate the non-client area
                SetWindowPos(hwnd, nullptr, 0,0,0,0,
                            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

                // Get the dialog window's dimensions
                RECT rcDlg;
                GetWindowRect(hwnd, &rcDlg);
                int dlgWidth = rcDlg.right - rcDlg.left;
                int dlgHeight = rcDlg.bottom - rcDlg.top;

                MONITORINFO mi = { sizeof(mi) };
                HMONITOR hMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
                if (GetMonitorInfo(hMonitor, &mi))
                {
                    int centerX = (mi.rcWork.left + mi.rcWork.right - dlgWidth) / 2;
                    int centerY = (mi.rcWork.top + mi.rcWork.bottom - dlgHeight) / 2;
                    SetWindowPos(hwnd, nullptr, centerX, centerY, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
                    SetForegroundWindow(hwnd);
                }

                HWND hwndRithmicBitmap = GetDlgItem(hwnd, IDC_RITHMIC_LOGO);
                if (hwndRithmicBitmap)
                {
                    hRithmicBmp = LoadBitmapFromDll(g_hThisModule, IDI_RITHMIC_LOGO);
                    if (hRithmicBmp)
                    {
                        SendMessageW(hwndRithmicBitmap, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hRithmicBmp);
                    }
                }

                HWND hwndOmneBitmap = GetDlgItem(hwnd, IDC_OMNE_LOGO);
                if (hwndOmneBitmap)
                {
                    hOmneBmp = LoadBitmapFromDll(g_hThisModule, IDI_OMNE_LOGO);
                    if (hOmneBmp)
                    {
                        SendMessageW(hwndOmneBitmap, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hOmneBmp);
                    }
                }
                return TRUE;
            }

            case WM_TIMER:
                if (wParam == IDT_AUTOCLOSE_TIMER)
                {
                    KillTimer(hwnd, IDT_AUTOCLOSE_TIMER);
                    DestroyWindow(hwnd);
                    return TRUE;
                }
                break;

            case WM_CLOSE:
                return TRUE;

            case WM_DESTROY:
                if (hRithmicBmp) { DeleteObject(hRithmicBmp); hRithmicBmp = NULL; }
                if (hOmneBmp) { DeleteObject(hOmneBmp); hOmneBmp = NULL; }
                return TRUE;
        }
        return FALSE;
    }
}

void ShowCopyrightDialog() 
{
    HWND hDlg = CreateDialog(g_hThisModule, MAKEINTRESOURCE(IDD_COPYRIGHT_DIALOG), NULL, CopyrightDialogProc);
    if (hDlg)
    {
        ShowWindow(hDlg, SW_SHOW);

        MSG msg;
        while (IsWindow(hDlg) && GetMessage(&msg, nullptr, 0, 0))
        {
            if (!IsDialogMessage(hDlg, &msg))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
    }
}