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

#define IDT_REPOS_TIMER 1

extern HINSTANCE g_hThisModule;

namespace {
    static HWND g_hDlg = nullptr;
    static int g_dlgX = 0;
    static int g_dlgY = 0;

    HBITMAP LoadBitmapFromDll(HINSTANCE hInst, int resourceId)
    {
        return (HBITMAP)LoadImageW(hInst,
                                    MAKEINTRESOURCEW(resourceId),
                                    IMAGE_BITMAP,
                                    0, 0,
                                    LR_DEFAULTSIZE);
    }

    void PositionDialog()
    {
        auto hParent = zorro::Global::get().handle_;
        if (! IsWindow(hParent) || ! IsWindow(g_hDlg))
        {
            return;
        }

        RECT rcP, rcD;
        GetWindowRect(hParent, &rcP);
        GetWindowRect(g_hDlg,    &rcD);

        int parentW = rcP.right  - rcP.left;
        int dialogW = rcD.right  - rcD.left;

        // center horizontally over parent
        int x = rcP.left + (parentW - dialogW) / 2;
        // place dialog just *below* parent's bottom edge
        int y = rcP.bottom - 10;

        if (g_dlgX != x || g_dlgY != y)
        {
            g_dlgX = x;
            g_dlgY = y;

            SetWindowPos(g_hDlg,
                        HWND_TOP,    // keep it above non-topmost windows
                        x, y,       // new pos
                        0, 0,
                        SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        }
    }

    INT_PTR CALLBACK LogosDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        static HBITMAP hRithmicBmp = NULL;
        static HBITMAP hOmneBmp = NULL;

        switch (message)
        {
            case WM_INITDIALOG:
            {
                // remove title bar, system menu, thick frame
                LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
                style &= ~(WS_CAPTION | WS_SYSMENU | WS_THICKFRAME);
                SetWindowLongPtr(hwnd, GWL_STYLE, style);

                // tell Windows to recalculate the non-client area
                SetWindowPos(hwnd, nullptr, 0,0,0,0,
                            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

                SetTimer(hwnd, IDT_REPOS_TIMER, 100, nullptr);

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
                if (wParam == IDT_REPOS_TIMER)
                {
                    PositionDialog();
                    if (GetForegroundWindow() == zorro::Global::get().handle_)
                    {
                        SetWindowPos(g_hDlg, HWND_TOPMOST,
                                    0,0,0,0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
                        // immediately revert back to normal
                        SetWindowPos(g_hDlg, HWND_NOTOPMOST,
                                    0,0,0,0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
                    }
                }
                break;

            case WM_CLOSE:
                return TRUE;

            case WM_DESTROY:
                KillTimer(hwnd, IDT_REPOS_TIMER);
                if (hRithmicBmp) { DeleteObject(hRithmicBmp); hRithmicBmp = NULL; }
                if (hOmneBmp) { DeleteObject(hOmneBmp); hOmneBmp = NULL; }
                return TRUE;
        }
        return FALSE;
    }
}

void ShowRithmicLogosDialog() 
{
    std::thread ([]() {
        g_hDlg = CreateDialog(g_hThisModule, MAKEINTRESOURCE(IDD_LOGOS_DIALOG), NULL, LogosDialogProc);
        if (!g_hDlg)
        {
            return;
        }

        PositionDialog();
        ShowWindow(g_hDlg, SW_SHOW);

        MSG msg;
        while (IsWindow(g_hDlg) && GetMessage(&msg, nullptr, 0, 0))
        {
            if (!IsDialogMessage(g_hDlg, &msg))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
    }).detach();
}

void CloseRithmicLogosDialog()
{
    if (g_hDlg)
    {
        DestroyWindow(g_hDlg);
        g_hDlg = nullptr;
    }
}