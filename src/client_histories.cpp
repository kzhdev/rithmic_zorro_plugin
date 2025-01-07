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
#include "global.h"

using namespace zorro;
using namespace RApi;

namespace {
    auto &global = Global::get();
}

const std::vector<T6>& RithmicClient::replayBars(char* asset, int64_t start, int64_t end, int n_tick_minutes, int n_ticks)
{
    ticks_.clear();
    std::string exchange;
    std::string ticker;

    auto *symbol = getSymbol(asset);
    if (!symbol)
    {
        std::string_view _asset(asset);
        auto pos = _asset.find_last_of(".");

        if (pos == std::string::npos)
        {
            BrokerError(std::format("Invalid Asset {}. A valid asset must be <Ticker>.<Exchange>", asset).c_str());
            return ticks_;
        }

        exchange = _asset.substr(0, pos);
        ticker = _asset.substr(pos + 1);
    }
    else
    {
        exchange = symbol->spec_.exchange_;
        ticker = symbol->spec_.ticker_;
    }

    ReplayBarParams params;
    params.sExchange.pData = (char*)exchange.data();
    params.sExchange.iDataLen = (int)exchange.length();
    params.sTicker.pData = (char*)ticker.data();
    params.sTicker.iDataLen = (int)ticker.length();

    params.iType = BAR_TYPE_MINUTE;
    params.iSpecifiedMinutes = n_tick_minutes;

    if (n_tick_minutes == 864000)
    {
        params.iType = BAR_TYPE_DAILY;

        // daily bar
        char start_date[8] {'\0'};
        char end_date[8] {'\0'};
        tm* utc_start = gmtime((time_t*)&start);
        tm* utc_end = gmtime((time_t*)&end);
        strftime(start_date, 8, "%Y%m%d", utc_start);
        strftime(end_date, 8, "%Y%m%d", utc_end);
        params.sStartDate.pData = start_date;
        params.sStartDate.iDataLen = 8;
        params.sEndDate.pData = end_date;
        params.sEndDate.iDataLen = 8;
    }
    else
    {
        params.iStartSsboe = start;
        params.iEndSsboe = end;
    }

    n_ticks_requested_ = n_ticks;
    request_status_.store(RequestStatus::AwaitingResults, std::memory_order_relaxed);
    int i_code;
    if (!engine_->replayBars(&params, &i_code))
    {
        BrokerError(std::format("REngine::replayBars() err: {}", i_code).c_str());
        if (i_code == API_NO_DATA)
        {
            global.asset_no_data_.emplace(asset);
        }
        return ticks_;
    }

    auto status = waitForRequest();
    // Zorro requests bars in desending order
    std::reverse(ticks_.begin(), ticks_.end());
    return ticks_;
}

int RithmicClient::Bar(RApi::BarInfo *pInfo, void *pContext, int *aiCode)
{
    if (ticks_.size() < n_ticks_requested_)
    {
        auto &ticker = ticks_.emplace_back();
        // SPDLOG_TRACE("Bar {} {}-{} {} {} {} {} {}", symbol(pInfo), pInfo->iStartSsboe * 1000000000 + pInfo->iStartUsecs * 1000,
        //     pInfo->iEndSsboe * 1000000000 + pInfo->iEndUsecs * 1000, pInfo->dOpenPrice, pInfo->dHighPrice, pInfo->dLowPrice, pInfo->dClosePrice, pInfo->llVolume);
        ticker.time = convertTime((__time32_t)pInfo->iEndSsboe);    // Zorro's bar time is the end of the bar
        ticker.fOpen = pInfo->dOpenPrice;
        ticker.fHigh = pInfo->dHighPrice;
        ticker.fLow = pInfo->dLowPrice;
        ticker.fClose = pInfo->dClosePrice;
        ticker.fVol = pInfo->llVolume;
    }
    *aiCode = API_OK;
    return (OK);
}

int RithmicClient::BarReplay(RApi::BarReplayInfo *pInfo, void *pContext, int *aiCode)
{
    if (pInfo->iRpCode != API_OK)
    {
        SPDLOG_DEBUG("BarReplay err: {}", to_string_view(pInfo->sRpCode));
    }
    request_status_.store(RequestStatus::Complete, std::memory_order_release);
    *aiCode = API_OK;
    return (OK);
}

