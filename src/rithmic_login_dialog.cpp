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

static constexpr LPCWSTR REG_PATH    = L"Software\\RithmicZorroPlugin\\RithmicLogin";
static constexpr LPCWSTR REG_VAL_SRV = L"LastServer";
static constexpr LPCWSTR REG_VAL_GWY = L"LastGateway";

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
        const RithmicSystemConfig::ServerGatewayT& server_gateways;
        std::string selected_server;
        std::string selected_gateway;

        DialogData(const RithmicSystemConfig::ServerGatewayT& _server_gateways)
            : server_gateways(_server_gateways)
        {
        }
    };

    void populateGateway(DialogData *data, int serverIndex, LPCWSTR pick, HWND server_combo, HWND gateway_combo)
    {
        // get server name
        WCHAR serverName[256];
        SendMessageW(server_combo, CB_GETLBTEXT, serverIndex, (LPARAM)serverName);

        // Convert to std::string
        char buf[256];
        WideCharToMultiByte(CP_ACP, 0, serverName, -1, buf, sizeof(buf), NULL, NULL);
        std::string server(buf);

        // clear + fill
        SendMessage(gateway_combo, CB_RESETCONTENT, 0, 0);
        auto it = data->server_gateways.find(server);
        if (it != data->server_gateways.end())
        {
            for (auto& gw : it->second)
            {
                std::wstring wgw(gw.begin(), gw.end());
                SendMessageW(gateway_combo, CB_ADDSTRING, 0, (LPARAM)wgw.c_str());
            }
        }

        // select savedGateway if matches, otherwise first
        int sel = CB_ERR;
        if (pick)
        {
            sel = (int)SendMessageW(gateway_combo, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)pick);
        }
        if (sel == CB_ERR)
        {
            sel = 0;
        }
        SendMessage(gateway_combo, CB_SETCURSEL, sel, 0);
    }


    INT_PTR CALLBACK LoginDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        static DialogData* data;
        static HBITMAP hRithmicBitmap = nullptr;
        static HBITMAP hOmneBitmap = nullptr;
        static std::wstring savedServer, savedGateway;

        switch (message)
        {
            case WM_INITDIALOG:
            {
                data = reinterpret_cast<DialogData*>(lParam);
                HWND server_combo = GetDlgItem(hwnd, IDC_SERVER_COMBO);
                if (!server_combo) [[unlikely]]
                {
                    SPDLOG_ERROR("Failed to get IDC_SERVER_COMBO");
                    return FALSE;
                }
                HWND gateway_combo = GetDlgItem(hwnd, IDC_GATEWAY_COMBO);
                if (!gateway_combo) [[unlikely]]
                {
                    SPDLOG_ERROR("Failed to get IDC_GATEWAY_COMBO");
                    return FALSE;
                }
                // Populate servers combo
                for (const auto& kvp : data->server_gateways)
                {
                    std::wstring wideServer(kvp.first.begin(), kvp.first.end());
                    SendMessageW(server_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wideServer.c_str()));
                }

                // Read saved values from registry
                HKEY hKey = nullptr;
                if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_PATH, 0, KEY_READ, &hKey) == ERROR_SUCCESS)
                {
                    // Server
                    WCHAR buf[256]; DWORD cb = sizeof(buf);
                    if (RegQueryValueExW(hKey, REG_VAL_SRV, nullptr, nullptr, (LPBYTE)buf, &cb) == ERROR_SUCCESS)
                    {
                        savedServer = buf;
                    }

                    // Gateway
                    cb = sizeof(buf);
                    if (RegQueryValueExW(hKey, REG_VAL_GWY, nullptr, nullptr, (LPBYTE)buf, &cb) == ERROR_SUCCESS)
                    {
                        savedGateway = buf;
                    }

                    RegCloseKey(hKey);
                }

                // Select the saved server (or index 0 if not found)
                int srvIndex = CB_ERR;
                if (!savedServer.empty())
                {
                    srvIndex = (int)SendMessageW(server_combo, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)savedServer.c_str());
                }
                if (srvIndex == CB_ERR) 
                {
                    srvIndex = 0;
                }
                SendMessage(server_combo, CB_SETCURSEL, srvIndex, 0);
                
                populateGateway(data, srvIndex, savedGateway.empty() ? nullptr : savedGateway.c_str(), server_combo, gateway_combo);

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
            {
                int id    = LOWORD(wParam);
                int event = HIWORD(wParam);

                if (id == IDC_SERVER_COMBO && event == CBN_SELCHANGE)
                {
                    HWND server_combo  = GetDlgItem(hwnd, IDC_SERVER_COMBO);
                    HWND gateway_combo = GetDlgItem(hwnd, IDC_GATEWAY_COMBO);

                    // Get selected server text
                    int sel = SendMessage(server_combo, CB_GETCURSEL, 0, 0);
                    if (sel != CB_ERR)
                    {
                        wchar_t wbuf[256];
                        SendMessageW(server_combo, CB_GETLBTEXT, sel, reinterpret_cast<LPARAM>(wbuf));

                        // Convert to std::string
                        char buf[256];
                        WideCharToMultiByte(CP_ACP, 0, wbuf, -1, buf, sizeof(buf), NULL, NULL);
                        std::string server(buf);

                        if (!savedServer.empty() && savedServer == std::wstring(wbuf))
                        {
                            // If the saved server matches the selected one, use savedGateway
                            sel = (int)SendMessageW(gateway_combo, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)savedGateway.c_str());
                        }
                        else
                        {
                            sel = 0; // Default to first gateway if no match
                        }

                        // Clear existing gateways
                        SendMessage(gateway_combo, CB_RESETCONTENT, 0, 0);

                        // Repopulate gateways for selected server
                        auto it = data->server_gateways.find(server);
                        if (it != data->server_gateways.end())
                        {
                            for (const auto& gateway : it->second)
                            {
                                std::wstring wideGateway(gateway.begin(), gateway.end());
                                SendMessageW(gateway_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wideGateway.c_str()));
                            }
                            SendMessage(gateway_combo, CB_SETCURSEL, sel, 0);
                        }
                    }
                }
                else if (id == IDOK)
                {
                    // retrieve current selections
                    HWND sc = GetDlgItem(hwnd, IDC_SERVER_COMBO);
                    HWND gc = GetDlgItem(hwnd, IDC_GATEWAY_COMBO);
                    wchar_t wsrv[256] = {}, wgw[256] = {};
                    int si = (int)SendMessage(sc, CB_GETCURSEL, 0, 0);
                    if (si != CB_ERR)
                    {
                        SendMessageW(sc, CB_GETLBTEXT, si, (LPARAM)wsrv);
                    }
                    int gi = (int)SendMessage(gc, CB_GETCURSEL, 0, 0);
                    if (gi != CB_ERR)
                    {
                        SendMessageW(gc, CB_GETLBTEXT, gi, (LPARAM)wgw);
                    }

                    // save to registry
                    HKEY hKey = nullptr;
                    if (RegCreateKeyExW(HKEY_CURRENT_USER, REG_PATH, 0, nullptr,
                                        REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS)
                    {
                        RegSetValueExW(hKey, REG_VAL_SRV, 0, REG_SZ,
                                    (const BYTE*)wsrv,
                                    (DWORD)((wcslen(wsrv) + 1) * sizeof(wchar_t)));
                        RegSetValueExW(hKey, REG_VAL_GWY, 0, REG_SZ,
                                    (const BYTE*)wgw,
                                    (DWORD)((wcslen(wgw) + 1) * sizeof(wchar_t)));
                        RegCloseKey(hKey);
                    }

                    // Store selected server and gateway in data
                    {
                        // convert back to std::string
                        char buf[256];
                        WideCharToMultiByte(CP_ACP, 0, wsrv, -1, buf, _countof(buf), NULL, NULL);
                        data->selected_server = buf;
                        WideCharToMultiByte(CP_ACP, 0, wgw, -1, buf, _countof(buf), NULL, NULL);
                        data->selected_gateway = buf;
                    }

                    EndDialog(hwnd, IDOK);
                    return TRUE;
                }
                break;
            }

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

std::string ShowLoginDialog(const RithmicSystemConfig::ServerGatewayT &server_gateways) 
{
    DialogData data(server_gateways);
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
        return std::format("{}_{}", data.selected_server, data.selected_gateway);
    }
    return std::string();
}