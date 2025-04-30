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
#include "utils.h"

using namespace zorro;
using namespace RApi;

bool RithmicClient::subscribePnl()
{
    request_status_.store(RequestStatus::AwaitingResults, std::memory_order_relaxed);
    int iCode;
    if (!engine_->subscribePnl(&account_info_, &iCode))
    {
        BrokerError(std::format("REngine::subscribePnl() err: {}", iCode).c_str());
        return false;
    }

    if (!engine_->replayPnl(&account_info_, &iCode))
    {
        BrokerError(std::format("REngine::replayPnl() err: {}", iCode).c_str());
        return false;
    }

    auto status = waitForRequest(10000);
    if (status == RequestStatus::Timeout)
    {
        BrokerError("REngine::replayPnl() timeout");
        return true;
    }

    return status == RequestStatus::Complete;
}

int RithmicClient::PnlReplay(RApi::PnlReplayInfo *pInfo, void *pContext, int *aiCode)
{
    if (pInfo->iArrayLen)
    {
        double trade_value = 0.;
        for (auto i = 0; i < pInfo->iArrayLen; ++i)
        {
            auto &pnl_info = pInfo->asPnlInfoArray[i];
            handlePnlInfo(pnl_info);
        }
    }
    request_status_.store(RequestStatus::Complete, std::memory_order_release);
    *aiCode = API_OK;
    return OK;
}

int RithmicClient::PnlUpdate(RApi::PnlInfo *pInfo, void *pContext, int *aiCode)
{
    handlePnlInfo(*pInfo);
    *aiCode = API_OK;
    return OK;
}

void RithmicClient::handlePnlInfo(const RApi::PnlInfo &pnl_info)
{
    auto iter = asset_position_.find(symbol(&pnl_info));
    if (iter == asset_position_.end())
    {
        return;
    }

    auto timestamp = nanosec(pnl_info);

    auto pnl = pnl_.load(std::memory_order_relaxed);
    PnL new_pnl;
    do
    {
        if (pnl.timestamp_ > timestamp)
        {
            break;
        }
        new_pnl.timestamp_ = timestamp;

        if (pnl_info.eOpenPnl == VALUE_STATE_CLEAR)
        {
            new_pnl.unrealized_pnl_ = 0.;
        }
        else if (pnl_info.eOpenPnl == VALUE_STATE_USE)
        {
            new_pnl.unrealized_pnl_ = pnl_info.dOpenPnl;
        }
        else
        {
            new_pnl.unrealized_pnl_ = pnl.unrealized_pnl_;
        }

        if (pnl_info.eClosedPnl == VALUE_STATE_CLEAR)
        {
            new_pnl.realized_pnl_ = 0.;
        }
        else if (pnl_info.eClosedPnl == VALUE_STATE_USE)
        {
            new_pnl.realized_pnl_ = pnl_info.dClosedPnl;
        }
        else
        {
            new_pnl.realized_pnl_ = pnl.realized_pnl_;
        }

        new_pnl.pnl_ = new_pnl.realized_pnl_ + new_pnl.unrealized_pnl_;

        if (pnl_info.eAccountBalance == VALUE_STATE_CLEAR)
        {
            new_pnl.account_balance_ = 0.;
        }
        else if (pnl_info.eAccountBalance == VALUE_STATE_USE)
        {   
            new_pnl.account_balance_ = pnl_info.dAccountBalance;
        }
        else
        {
            new_pnl.account_balance_ = pnl.account_balance_;
        }
    } while (!pnl_.compare_exchange_weak(pnl, new_pnl, std::memory_order_release, std::memory_order_relaxed));

    auto position = iter->second.load(std::memory_order_relaxed);
    Position new_position;
    do
    {
        if (timestamp < position.timestamp_)
        {
            return;
        }

        new_position.timestamp_ = timestamp;

        if (pnl_info.eAvgOpenFillPrice == VALUE_STATE_CLEAR)
        {
            new_position.average_price_ = 0.;
        }
        else if (pnl_info.eAvgOpenFillPrice == VALUE_STATE_USE)
        {
            new_position.average_price_ = pnl_info.dAvgOpenFillPrice;
        }
        else
        {
            new_position.average_price_ = position.average_price_;
        }

        if (pnl_info.ePosition == VALUE_STATE_CLEAR)
        {
            new_position.quantity_ = 0;
        }
        else if (pnl_info.ePosition == VALUE_STATE_USE)
        {
            new_position.quantity_ = pnl_info.llPosition;
        }
        else
        {
            new_position.quantity_ = position.quantity_;
        }

        if (pnl_info.eBuyQty == VALUE_STATE_CLEAR)
        {
            new_position.buy_qty_ = 0;
        }
        else if (pnl_info.eBuyQty == VALUE_STATE_USE)
        {
            new_position.buy_qty_ = pnl_info.llBuyQty;
        }
        else
        {
            new_position.buy_qty_ = position.buy_qty_;
        }

        if (pnl_info.eSellQty == VALUE_STATE_CLEAR)
        {
            new_position.sell_qty_ = 0;
        }
        else if (pnl_info.eSellQty == VALUE_STATE_USE)
        {
            new_position.sell_qty_ = pnl_info.llSellQty;
        }
        else
        {
            new_position.sell_qty_ = position.sell_qty_;
        }

    } while (!iter->second.compare_exchange_weak(position, new_position, std::memory_order_release, std::memory_order_relaxed));
    SPDLOG_TRACE("{} position={}, pnl={}", iter->first, new_position.quantity_, new_pnl.pnl_);
}

Position RithmicClient::getPosition(const char *asset) const
{
    auto iter = asset_position_.find(asset);
    if (iter != asset_position_.end())
    {
        return iter->second.load(std::memory_order_relaxed);
    }
    return {};
}