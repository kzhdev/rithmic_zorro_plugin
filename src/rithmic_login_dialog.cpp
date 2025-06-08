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
#include "rithmic_login_dialog.h"
#include <windows.h>
#include <resource.h>
#include "global.h"
#include <string>
#include <vector>

using namespace zorro;

namespace zorro
{
    extern int(__cdecl* BrokerError)(const char* txt);
}

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

    struct DialogData
    {
        const std::vector<std::string>& servers;
        std::string selected_server;

        DialogData(const std::vector<std::string>& server_names)
            : servers(server_names)
        {
        }
    };

    INT_PTR CALLBACK LoginDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        static DialogData* data;
        static HBITMAP hRithmicBitmap = nullptr;
        static HBITMAP hOmneBitmap = nullptr;

        switch (message)
        {
            case WM_INITDIALOG:
            {
                data = reinterpret_cast<DialogData*>(lParam);
                HWND combo = GetDlgItem(hwnd, IDC_SERVER_COMBO);
                if (!combo)
                {
                    SPDLOG_ERROR("Failed to get IDC_SERVER_COMBO");
                    return FALSE;
                }
                for (const auto& server : data->servers)
                {
                    std::wstring wideServer(server.begin(), server.end());
                    SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wideServer.c_str()));
                }
                SendMessage(combo, CB_SETCURSEL, 0, 0);

                HWND hwndRithmicBitmap = GetDlgItem(hwnd, IDC_RITHMIC_LOGO);
                if (hwndRithmicBitmap)
                {
                    hRithmicBitmap = LoadBitmapFromDll(g_hThisModule, IDI_RITHMIC_LOGO);
                    if (hRithmicBitmap)
                    {
                        SendMessageW(hwndRithmicBitmap, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hRithmicBitmap);
                    }
                    else
                    {
                        SPDLOG_ERROR("Failed to load bitmap IDI_RITHMIC_LOGO: {}", GetLastError());
                    }
                }
                else
                {
                    SPDLOG_ERROR("Failed to get IDI_RITHMIC_LOGO control");
                }

                HWND hwndOmneBitmap = GetDlgItem(hwnd, IDC_OMNE_LOGO);
                if (hwndOmneBitmap)
                {
                    hOmneBitmap = LoadBitmapFromDll(g_hThisModule, IDI_OMNE_LOGO);
                    if (hOmneBitmap)
                    {
                        SendMessageW(hwndOmneBitmap, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hOmneBitmap);
                    }
                    else
                    {
                        SPDLOG_ERROR("Failed to load bitmap IDI_OMNE_LOGO: {}", GetLastError());
                    }
                }
                else
                {
                    SPDLOG_ERROR("Failed to get IDC_OMNE_LOGO control");
                }
                return TRUE;
            }

            case WM_COMMAND:
                if (LOWORD(wParam) == IDOK)
                {
                    HWND combo = GetDlgItem(hwnd, IDC_SERVER_COMBO);
                    int index = SendMessage(combo, CB_GETCURSEL, 0, 0);
                    if (index != CB_ERR)
                    {
                        char buffer[256];
                        wchar_t wbuffer[256];
                        SendMessageW(combo, CB_GETLBTEXT, index, reinterpret_cast<LPARAM>(wbuffer));
                        WideCharToMultiByte(CP_ACP, 0, wbuffer, -1, buffer, 256, NULL, NULL);
                        data->selected_server = buffer;
                    }
                    EndDialog(hwnd, IDOK);
                    return TRUE;
                }
                break;

            case WM_CLOSE:
                EndDialog(hwnd, 0);
                return TRUE;

            case WM_DESTROY:
                if (hRithmicBitmap)
                {
                    DeleteObject(hRithmicBitmap);
                    hRithmicBitmap = nullptr;
                }
                if (hOmneBitmap)
                {
                    DeleteObject(hOmneBitmap);
                    hOmneBitmap = nullptr;
                }
                return TRUE;
        }
        return FALSE;
    }
}

std::string ShowLoginDialog(const std::vector<std::string>& servers) 
{
    DialogData data(servers);
    INT_PTR result = DialogBoxParam(g_hThisModule, MAKEINTRESOURCE(IDD_LOGIN_DIALOG), zorro::Global::get().handle_, LoginDialogProc, reinterpret_cast<LPARAM>(&data));
    if (result == -1)
    {
        BrokerError(std::format("DialogBoxParam failed with error: {}", GetLastError()).c_str());
        SPDLOG_ERROR("DialogBoxParam failed with error: {}", GetLastError());
        MessageBoxW(NULL, L"DialogBoxParam failed", L"Error", MB_OK | MB_ICONERROR);
        return std::string();
    }

    if (result == IDOK && !data.selected_server.empty())
    {
        return data.selected_server;
    }
    return std::string();
}