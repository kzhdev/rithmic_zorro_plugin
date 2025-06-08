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

// dllmain.cpp : Defines the entry point for the DLL application.
#include "stdafx.h"
#include "rithmic_copyright_dialog.h"

static const wchar_t* kShownEventName = L"Local\\ZorroCopyrightShown";
HINSTANCE g_hThisModule = nullptr;

HWND FindMainWindow()
{
    HWND hwndFound = nullptr;

    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        DWORD wndPID;
        GetWindowThreadProcessId(hwnd, &wndPID);

        if (wndPID == GetCurrentProcessId() && GetWindow(hwnd, GW_OWNER) == nullptr && IsWindowVisible(hwnd))
        {
            *reinterpret_cast<HWND*>(lParam) = hwnd;
            return FALSE; // stop enumeration
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&hwndFound));

    return hwndFound;
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved)
{
    static LONG shown = 0;

    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    {
        g_hThisModule = hModule;
        // Try to open an existing named Event
        HANDLE hEvt = OpenEvent(EVENT_MODIFY_STATE, FALSE, kShownEventName);
        if (!hEvt)
        {
            // -- first time in this process --
            // 1) Create the event so future loads will see it
            HANDLE hCreated = CreateEvent(
                nullptr,    // default security
                TRUE,       // manual-reset
                FALSE,      // initial non-signaled
                kShownEventName);
            // (we deliberately do NOT CloseHandle(hCreated) here —
            //  the handle will stay open in the process’s handle table
            //  so the Event object sticks around.)

            // 2) Spawn copyright dialog off the loader lock
            std::thread([](){
                ShowCopyrightDialog();
                HWND hMain = FindMainWindow();
                if (IsWindow(hMain))
                {
                    // bring the main window to the foreground
                    SetForegroundWindow(hMain);
                }
            }).detach();
        }
        else
        {
            // Already shown once — clean up our temporary open handle
            CloseHandle(hEvt);
        }
        break;
    }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

