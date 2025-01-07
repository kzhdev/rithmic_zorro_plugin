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

int RithmicClient::TradePrint(RApi::TradeInfo *pInfo, void *pContext, int *aiCode)
{
    if (pInfo->bPriceFlag)
    {
        auto sym = symbol(pInfo);
        auto iter = symbols_.find(sym);
        if (iter != symbols_.end())
        {
            auto &symbol = iter->second;
            auto trade = symbol.last_trade_.load(std::memory_order_relaxed);
            Trade new_trade;
            new_trade.side_ = to_side(*pInfo);
            new_trade.price_ = pInfo->dPrice;
            new_trade.qty_ = pInfo->llSize;
            new_trade.time_ = nanosec(*pInfo);
            if (pInfo->bVolumeBoughtFlag)
            {
                new_trade.buy_volume_ = pInfo->llVolumeBought;
            }
            else
            {
                new_trade.buy_volume_ = trade.buy_volume_;
            }
            if (pInfo->bVolumeSoldFlag)
            {
                new_trade.sell_volume_ = pInfo->llVolumeSold;
            }
            else
            {
                new_trade.sell_volume_ = trade.sell_volume_;
            }
            while (!symbol.last_trade_.compare_exchange_weak(trade, new_trade, std::memory_order_release, std::memory_order_relaxed));
        }
    }
    *aiCode = API_OK;
    return OK;
}