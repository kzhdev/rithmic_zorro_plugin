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

RithmicClient::RequestStatus RithmicClient::waitForRequest(uint32_t timeout_ms)
{
    RequestStatus status;
    if (timeout_ms > 0)
    {
        auto start = get_timestamp();
        while ((status = request_status_.load(std::memory_order_relaxed)) == RequestStatus::AwaitingResults)
        {
            if (!BrokerProgress(1))
            {
                status = RequestStatus::Failed;
                break;
            }
            if ((get_timestamp() - start) > timeout_ms)
            {
                status = RequestStatus::Timeout;
                break;
            }
        }
    }
    else
    {
        while((status = request_status_.load(std::memory_order_relaxed)) == RequestStatus::AwaitingResults)
        {
            if (!BrokerProgress(1))
            {
                status = RequestStatus::Failed;
                break;
            }
        }
    }
    request_status_.store(RequestStatus::NoRequest, std::memory_order_relaxed);
    return status;
}

bool RithmicClient::getRefData(tsNCharcb &exchange, tsNCharcb &symbol)
{
    int icode;
    request_status_.store(RequestStatus::AwaitingResults, std::memory_order_relaxed);
    if (!engine_->getRefData(&exchange, &symbol, &icode))
    {
        BrokerError(std::format("REngine::getRefData() err: {}", icode).c_str());
        return false;
    }

    auto status = waitForRequest();
    return status == RequestStatus::Complete;
}

bool RithmicClient::getPriceIncInfo(tsNCharcb &exchange, tsNCharcb &symbol)
{
    int icode;
    request_status_.store(RequestStatus::AwaitingResults, std::memory_order_relaxed);
    if (!engine_->getPriceIncrInfo(&exchange, &symbol, &icode))
    {
        BrokerError(std::format("REngine::getPriceIncInfo() err: {}", icode).c_str());
        return false;
    }

    auto status = waitForRequest();
    return status == RequestStatus::Complete;
}

Symbol* RithmicClient::getSymbol(const char* asset)
{
    auto iter = symbols_.find(asset);
    if (iter != symbols_.end())
    {
        return &(iter->second);
    }
    return nullptr;
}

bool RithmicClient::subscribe(const char* asset)
{
    SPDLOG_INFO("subscribe {}", asset);

    std::string_view _asset(asset);
    auto pos = _asset.find_last_of(".");

    if (pos == std::string::npos)
    {
        BrokerError(std::format("Invalid symbol {}. A valid symbol must be end with .<EXCHANGE>", asset).c_str());
        return false;
    }

    auto str_ticker = _asset.substr(0, pos);
    auto str_exchange = _asset.substr(pos + 1);

    asset_position_.emplace(asset, Position{});

    auto &sym = symbols_[asset];
    sym.spec_.symbol_ = asset;
    sym.spec_.ticker_ = str_ticker;
    sym.spec_.exchange_ = str_exchange;

    tsNCharcb exchange{(char*)str_exchange.data(), (int)str_exchange.length()};
    tsNCharcb ticker{(char*)str_ticker.data(), (int)str_ticker.length()};
    // if (!getRefData(exch, ticker))
    // {
    //     return false;
    // }

    int i_code;
    if (!engine_->subscribe(&exchange, &ticker, MD_PRINTS | MD_BEST | MD_MARKET_MODE, &i_code))
    {
        BrokerError(std::format("REngine::subscribe() err: {}", i_code).c_str());
        symbols_.erase(asset);
        return false;
    }
    return true;
}


int RithmicClient::RefData(RApi::RefDataInfo *pInfo, void *pContext, int *aiCode)
{
    std::string sym = symbol(pInfo);
    SPDLOG_DEBUG("RefData {}, bMinSizeIncrement={}, MinSizeIncrement={}, bSizeMultiplier={}, sizeMultiplier={}", sym, pInfo->bMinSizeIncrement, pInfo->llMinSizeIncrement, pInfo->bSizeMultiplier, pInfo->dSizeMultiplier);
    auto iter = symbols_.find(sym);
    if (iter != symbols_.end())
    {
        auto &symbol = iter->second;
        symbol.spec_.product_ = std::string(pInfo->sProductCode.pData, pInfo->sProductCode.iDataLen);
        symbol.spec_.tradable_ = pInfo->sIsTradable.iDataLen == 4;

        request_status_.store(RequestStatus::Complete, std::memory_order_release);
    }
    else
    {
        request_status_.store(RequestStatus::Failed, std::memory_order_release);
    }

    *aiCode = API_OK;
    return (OK);
}

int RithmicClient::PriceIncrUpdate(RApi::PriceIncrInfo *pInfo, void *pContext, int *aiCode)
{
    if (pInfo->iRpCode != API_OK)
    {
        SPDLOG_INFO("PriceIncrUpdate err: {}", to_string_view(pInfo->sRpCode));
        request_status_.store(RequestStatus::Failed, std::memory_order_release);
        *aiCode = API_OK;
        return (OK);
    }

    std::string sym = symbol(pInfo);
    auto iter = symbols_.find(sym);
    if (iter != symbols_.end())
    {
        auto &symbol = iter->second;
        for (auto i = 0; i < pInfo->iArrayLen; ++i)
        {
            auto &price_incr = pInfo->asPriceIncrArray[i];
            symbol.spec_.price_increment_ = price_incr.dPriceIncr;
        }
        request_status_.store(RequestStatus::Complete, std::memory_order_release);
    }
    else
    {
        request_status_.store(RequestStatus::Failed, std::memory_order_release);
    }

    *aiCode = API_OK;
    return (OK);
}

void RithmicClient::setMDReady(Symbol &symbol, MDReady flag)
{
    auto ready = symbol.ready_.load(std::memory_order_relaxed);
    std::bitset<3> new_ready;
    do
    {
        if (ready.test(flag))
        {
            // already set
            return;
        }
        new_ready = ready;
        new_ready.set(flag);
    } while(!symbol.ready_.compare_exchange_weak(ready, new_ready, std::memory_order_release, std::memory_order_relaxed));
    SPDLOG_INFO("{} {} ready. {}", symbol.spec_.symbol_, to_string(flag), new_ready.to_ulong());
}

int RithmicClient::BestAskQuote(RApi::AskInfo *pInfo, void *pContext, int *aiCode)
{
    std::string sym = symbol(pInfo);
    SPDLOG_TRACE("{} BestAsk {}@{}", sym, pInfo->bSizeFlag ? pInfo->llSize : 0, pInfo->bPriceFlag ? pInfo->dPrice : NAN);
    auto iter = symbols_.find(sym);
    if (iter != symbols_.end())
    {
        if (pInfo->bPriceFlag || pInfo->bSizeFlag)
        {
            MDTop top = iter->second.top_.load(std::memory_order_relaxed);
            bool was_nan = std::isnan(top.ask_price_);
            MDTop new_top;
            do
            {
                new_top = top;
                if (pInfo->bPriceFlag)
                {
                    new_top.ask_price_ = pInfo->dPrice;
                }
                if (pInfo->bSizeFlag)
                {
                    new_top.ask_qty_ = pInfo->llSize;
                }
            }
            while (!iter->second.top_.compare_exchange_weak(top, new_top, std::memory_order_release, std::memory_order_relaxed));
            if (was_nan && !std::isnan(new_top.ask_price_))
            {
                setMDReady(iter->second, MDReady::Top);
            }
            if (global.handle_ && global.price_type_.load(std::memory_order_relaxed) != 2)
            {
                PostMessage(global.handle_, WM_APP+1, 0, 0);
            }
        }
    }
    *aiCode = API_OK;
    return (OK);
}

int RithmicClient::BestBidAskQuote(RApi::BidInfo *pBid, RApi::AskInfo *pAsk, void *pContext, int *aiCode)
{
    std::string sym = symbol(pBid);
    SPDLOG_TRACE("{} BestBid: {}@{} BestAsk: {}@{}", sym, pBid->bSizeFlag ? pBid->llSize : 0, pBid->bPriceFlag ? pBid->dPrice : NAN, 
        pAsk->bSizeFlag ? pAsk->llSize : 0, pAsk->bPriceFlag ? pAsk->dPrice : NAN);
    auto iter = symbols_.find(sym);
    if (iter != symbols_.end())
    {
        if (pBid->bPriceFlag || pBid->bSizeFlag || pAsk->bPriceFlag || pAsk->bSizeFlag)
        {
            auto top = iter->second.top_.load(std::memory_order_relaxed);
            bool was_nan = std::isnan(top.ask_price_);
            MDTop new_top;
            do
            {
                new_top = top;
                if (pBid->bPriceFlag)
                {
                    new_top.bid_price_ = pBid->dPrice;
                }
                if (pBid->bSizeFlag)
                {
                    new_top.bid_qty_ = pBid->llSize;
                }
                if (pAsk->bPriceFlag)
                {
                    new_top.ask_price_ = pAsk->dPrice;
                }
                if (pAsk->bSizeFlag)
                {
                    new_top.ask_qty_ = pAsk->llSize;
                }
            }
            while (!iter->second.top_.compare_exchange_weak(top, new_top, std::memory_order_release, std::memory_order_relaxed));
            if (was_nan && !std::isnan(new_top.ask_price_))
            {
                setMDReady(iter->second, MDReady::Top);
            }
            if (global.handle_ && global.price_type_.load(std::memory_order_relaxed) != 2)
            {
                PostMessage(global.handle_, WM_APP+1, 0, 0);
            }
        }
    }
    *aiCode = API_OK;
    return (OK);
}

int RithmicClient::BestBidQuote(RApi::BidInfo *pInfo, void *pContext, int *aiCode)
{
    std::string sym = symbol(pInfo);
    SPDLOG_TRACE("{} BestBid {}@{}", sym, pInfo->bSizeFlag ? pInfo->llSize : 0, pInfo->bPriceFlag ? pInfo->dPrice : NAN);
    auto iter = symbols_.find(sym);
    if (iter != symbols_.end())
    {
        if (pInfo->bPriceFlag || pInfo->bSizeFlag)
        {
            auto top = iter->second.top_.load(std::memory_order_relaxed);
            MDTop new_top;
            do
            {
                new_top = top;
                if (pInfo->bPriceFlag)
                {
                    new_top.bid_price_ = pInfo->dPrice;
                }
                if (pInfo->bSizeFlag)
                {
                    new_top.bid_qty_ = pInfo->llSize;
                }
            }
            while (!iter->second.top_.compare_exchange_weak(top, new_top, std::memory_order_release, std::memory_order_relaxed));
            if (global.handle_ && global.price_type_.load(std::memory_order_relaxed) != 2)
            {
                PostMessage(global.handle_, WM_APP+1, 0, 0);
            }
        }
    }
    *aiCode = API_OK;
    return (OK);
}

int RithmicClient::MarketMode(RApi::MarketModeInfo *pInfo, void *pContext, int *aiCode)
{
    std::string sym = symbol(pInfo);
    SPDLOG_INFO("MarketMode: {} marketMode={} event={} reason={}", sym, std::string(pInfo->sMarketMode.pData, pInfo->sMarketMode.iDataLen),
        std::string(pInfo->sEvent.pData, pInfo->sEvent.iDataLen), std::string(pInfo->sReason.pData, pInfo->sReason.iDataLen));
    auto iter = symbols_.find(sym);
    if (iter != symbols_.end())
    {
        auto status = to_string_view(pInfo->sMarketMode);
        iter->second.can_trade_.exchange(status == "Open" || status == "Pre Open", std::memory_order_acq_rel);
    }
    setMDReady(iter->second, MDReady::Status);
    *aiCode = API_OK;
    return (OK);
}

std::vector<Spec> RithmicClient::searchInstrument(const std::string &search)
{
    auto iter = symbols_.find(search);
    if (iter != symbols_.end())
    {
        return {iter->second.spec_};
    }

    std::regex re("[\\-]");
    std::sregex_token_iterator first{search.begin(), search.end(), re, -1};   // the '-1' is what makes the regex split (-1 := what was not matched)
    std::sregex_token_iterator last;
    std::vector<std::string> result{first, last};

    if (result.size() < 3)
    {
        BrokerError("Invalid . <symobl>-<expiration>-<exchange>");
        return {};
    }
    return {};
}