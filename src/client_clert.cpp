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
#include "client.h"

using namespace RApi;
using namespace zorro;

int zorro::AdmCallbacks::Alert(RApi::AlertInfo *pInfo, void *pContext, int *aiCode)
{
    *aiCode = API_OK;
    return (OK);
}

int RithmicClient::Alert(RApi::AlertInfo *pInfo, void *pContext, int *aiCode)
{
    if (pInfo->iConnectionId == REPOSITORY_CONNECTION_ID)
    {
        if (pInfo->iAlertType == ALERT_LOGIN_COMPLETE)
        {
            request_status_.store(RequestStatus::Complete, std::memory_order_release);
        }
        else if (pInfo->iAlertType == ALERT_LOGIN_FAILED || pInfo->iAlertType == ALERT_CONNECTION_BROKEN)
        {
            request_status_.store(RequestStatus::Failed, std::memory_order_release);
        }
    }
    
    if (pInfo->iConnectionId == MARKET_DATA_CONNECTION_ID)
    {
        if (pInfo->iAlertType == ALERT_LOGIN_COMPLETE)
        {
            auto status = login_status_.load(std::memory_order_relaxed);
            std::bitset<LoginStatus::__Count__> new_status;
            do
            {
                new_status = status;
                new_status.set(LoginStatus::MarketDataComplete);
            }
            while (!login_status_.compare_exchange_weak(status, new_status, std::memory_order_release, std::memory_order_relaxed));
        }
        else if (pInfo->iAlertType == ALERT_LOGIN_FAILED || pInfo->iAlertType == ALERT_CONNECTION_BROKEN)
        {
            auto status = login_status_.load(std::memory_order_relaxed);
            std::bitset<LoginStatus::__Count__> new_status;
            do
            {
                new_status = status;
                new_status.set(LoginStatus::FailureOccured);
            }
            while (!login_status_.compare_exchange_weak(status, new_status, std::memory_order_release, std::memory_order_relaxed));
        }
    }

    if (pInfo->iConnectionId == TRADING_SYSTEM_CONNECTION_ID)
    {
        if (pInfo->iAlertType == ALERT_LOGIN_COMPLETE)
        {
            auto status = login_status_.load(std::memory_order_relaxed);
            std::bitset<LoginStatus::__Count__> new_status;
            do
            {
                new_status = status;
                new_status.set(LoginStatus::TradingSystemComplete);
            }
            while (!login_status_.compare_exchange_weak(status, new_status, std::memory_order_release, std::memory_order_relaxed));
        }
        else if (pInfo->iAlertType == ALERT_LOGIN_FAILED || pInfo->iAlertType == ALERT_CONNECTION_BROKEN)
        {
             auto status = login_status_.load(std::memory_order_relaxed);
            std::bitset<LoginStatus::__Count__> new_status;
            do
            {
                new_status = status;
                new_status.set(LoginStatus::FailureOccured);
            }
            while (!login_status_.compare_exchange_weak(status, new_status, std::memory_order_release, std::memory_order_relaxed));
        }
    }

    if (pInfo->iConnectionId == INTRADAY_HISTORY_CONNECTION_ID)
    {
        if (pInfo->iAlertType == ALERT_LOGIN_COMPLETE)
        {
            auto status = login_status_.load(std::memory_order_relaxed);
            std::bitset<LoginStatus::__Count__> new_status;
            do
            {
                new_status = status;
                new_status.set(LoginStatus::IntradayHistoryComplete);
            }
            while (!login_status_.compare_exchange_weak(status, new_status, std::memory_order_release, std::memory_order_relaxed));
        }
        else if (pInfo->iAlertType == ALERT_LOGIN_FAILED || pInfo->iAlertType == ALERT_CONNECTION_BROKEN)
        {
             auto status = login_status_.load(std::memory_order_relaxed);
            std::bitset<LoginStatus::__Count__> new_status;
            do
            {
                new_status = status;
                new_status.set(LoginStatus::FailureOccured);
            }
            while (!login_status_.compare_exchange_weak(status, new_status, std::memory_order_release, std::memory_order_relaxed));
        }
    }

    if (pInfo->iConnectionId == PNL_CONNECTION_ID)
    {
        if (pInfo->iAlertType == ALERT_LOGIN_COMPLETE)
        {
            auto status = login_status_.load(std::memory_order_relaxed);
            std::bitset<LoginStatus::__Count__> new_status;
            do
            {
                new_status = status;
                new_status.set(LoginStatus::PnLComplete);
            }
            while (!login_status_.compare_exchange_weak(status, new_status, std::memory_order_release, std::memory_order_relaxed));
        }
        else if (pInfo->iAlertType == ALERT_LOGIN_FAILED || pInfo->iAlertType == ALERT_CONNECTION_BROKEN)
        {
             auto status = login_status_.load(std::memory_order_relaxed);
            std::bitset<LoginStatus::__Count__> new_status;
            do
            {
                new_status = status;
                new_status.set(LoginStatus::FailureOccured);
            }
            while (!login_status_.compare_exchange_weak(status, new_status, std::memory_order_release, std::memory_order_relaxed));
        }
    }

    *aiCode = API_OK;
    return (OK);
}