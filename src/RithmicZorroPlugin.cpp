// The MIT License (MIT)
// Copyright (c) 2025 Kun Zhao
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software
// and associated documentation files (the "Software"), to deal in the Software without restriction,
// including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or substantial
// portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
// TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
// 

#include "stdafx.h"

#include "RithmicZorroPlugin.h"
#include "client.h"
#include "config.h"
#include "global.h"

// standard library
#include <type_traits>
#include "utils.h"
#include "pnl.h"

#define PLUGIN_VERSION	2

namespace {
    std::unique_ptr<zorro::RithmicClient> client_;
    void shutdown()
    {
        client_.reset();
        spdlog::shutdown();
    }

    auto &global = zorro::Global::get();
    auto &config = zorro::Config::get();
}

namespace zorro
{
    ////////////////////////////////////////////////////////////////
    DLLFUNC_C int BrokerOpen(char* Name, FARPROC fpError, FARPROC fpProgress)
    {
        strcpy_s(Name, 32, "Rithmic");
        (FARPROC&)BrokerError = fpError;
        (FARPROC&)BrokerProgress = fpProgress;
        return PLUGIN_VERSION;
    }

    DLLFUNC_C int BrokerLogin(char* User, char* Pwd, char* Type, char* Account)
    {
        if (!User) // log out
        {
            shutdown();
            return 0;
        }

        try
        {
            spdlog::init_thread_pool(8192, 1);
            auto async_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(std::format("./Log/rithmic_plugin_{}_{}.log", report(25), Type), 524288000, 5);
            auto async_logger = std::make_shared<spdlog::async_logger>("async_logger", async_sink, spdlog::thread_pool(), spdlog::async_overflow_policy::overrun_oldest);
            spdlog::set_default_logger(async_logger);
#ifdef DEBUG
            spdlog::set_level(spdlog::level::trace);
            spdlog::flush_on(spdlog::level::trace);
#else
            spdlog::set_level((spdlog::level::level_enum)config.get().log_level_);
            if (config.log_level_ > SPDLOG_LEVEL_INFO)
            {
                spdlog::flush_on((spdlog::level::level_enum)config.log_level_);
            }
#endif
            spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%F][%t][%l] %v");
            spdlog::flush_every(std::chrono::seconds(3));
            client_ = std::make_unique<RithmicClient>(User);
            global.reset();
        }
        catch(const std::exception& e)
        {
            BrokerError(e.what());
            shutdown();
            return 0;
        }

        std::string err;
        if (!client_->login(Pwd, err))
        {
            if (!err.empty())
            {
                BrokerError(err.c_str());
            }
            else
            {
                BrokerError("Login failed.");
            }

            shutdown();
            return 0;
        }

        SPDLOG_INFO("Login. Account: {}", client_->accountId());
        BrokerError(std::format("Account {}", client_->accountId()).c_str());
        sprintf_s(Account, 1024, client_->accountId().data());
        return 1;
    }

    DLLFUNC_C int BrokerAsset(char* Asset, double* pPrice, double* pSpread, double* pVolume, double* pPip, double* pPipCost, double* pLotAmount, double* pMarginCost, double* pRollLong, double* pRollShort)
    {
        auto *symbol = client_->getSymbol(Asset);
        if (!symbol)
        {
            if (!client_->subscribe(Asset))
            {
                return 0;
            }
            symbol = client_->getSymbol(Asset);
        }

        if (!pPrice)
        {
            return 1;
        }

        auto start = get_timestamp();
        while(symbol->ready_.load(std::memory_order_relaxed).to_ulong() < 3)
        {
            if (!BrokerProgress(1))
            {
                return 0;
            }

            if ((get_timestamp() - start) > 10000)
            {
                BrokerError(std::format("{} no data", Asset).c_str());
                return 0;
            }
        }

        auto top = symbol->top_.load(std::memory_order_relaxed);
        auto last_trade = symbol->last_trade_.load(std::memory_order_relaxed);
        if (global.price_type_.load(std::memory_order_relaxed) == 2)
        {
            if (!std::isnan(last_trade.price_))
            {
                *pPrice = last_trade.price_;
            }
            else
            {
                *pPrice = top.ask_price_;
            }
        }
        else
        {
            *pPrice = top.ask_price_;
        }

        if (pVolume)
        {
            switch(global.vol_type_)
            {
                case 0:
                    if (global.price_type_.load(std::memory_order_relaxed) == 2)
                    {
                        *pVolume = last_trade.buy_volume_ - last_trade.sell_volume_;
                    }
                    else
                    {
                        *pVolume = top.bid_qty_ + top.ask_qty_;
                    }
                    break;
                case 1:
                    // no volume
                    break;
                case 2:
                    // cumulative delta
                    *pVolume = last_trade.buy_volume_ - last_trade.sell_volume_;
                    break;
                case 3:
                    *pVolume = top.bid_qty_ + top.ask_qty_;
                    break;
                case 4:
                    *pVolume = last_trade.buy_volume_ - last_trade.sell_volume_;
                    break;
                case 5:
                    *pVolume = top.ask_price_;
                    break;
                case 6:
                    *pVolume = top.bid_price_;
            }
        }

        if (pSpread)
        {
            if (!std::isnan(top.bid_price_))
            {
                *pSpread = top.ask_price_ - top.bid_price_;
            }
            else
            {
                *pSpread = 0;
            }
        }
        return 1;
    }

    DLLFUNC_C int BrokerHistory2(char* Asset, DATE tStart, DATE tEnd, int nTickMinutes, int nTicks, T6* ticks)
    {
        if (global.asset_no_data_.find(Asset) != global.asset_no_data_.end())
        {
            return 0;
        }

        auto start = convertTime(tStart);
        auto end = convertTime(tEnd);
        SPDLOG_TRACE("BrokerHistory2 Asset={} tStart={}({}) tEnd={}({}) nTickMinutes={} nTicks={}", Asset, timeToString(start), start, timeToString(end), end, nTickMinutes, nTicks);
        const auto &downloaded = client_->replayBars(Asset, start, end, nTickMinutes, nTicks);
        if (!downloaded.empty())
        {
            SPDLOG_TRACE("downloaded {} bars. {} - {}", downloaded.size(), timeToString(convertTime(downloaded.front().time)), timeToString(convertTime(downloaded.back().time)));
            for (const auto &tick : downloaded)
            {
                *ticks = tick;
                ++ticks;
            }
        }
        return (int)downloaded.size();
    }

    DLLFUNC_C int BrokerAccount(char* Account, double* pdBalance, double* pdTradeVal, double* pdMarginVal)
    {
        if (pdBalance) {
            *pdBalance = client_->pnl().account_balance_;
        }
        return 0;
    }

    DLLFUNC_C int BrokerBuy2(char* Asset, int nAmount, double dStopDist, double dLimit, double* pPrice, int* pFill) 
    {
        SPDLOG_INFO("BrokerBuy2 {} nAmount={}({}) dStopDist={} dLimit={}({}) duration={}", Asset, nAmount, global.amount_, dStopDist, dLimit, global.limit_price_, to_string_view(global.order_duration_));

        if (!nAmount)
        {
            return 0;
        }

        // For market order, Zorro set the dLimit = 0 which could be a valid future contract price.
        // uset the global.limit_price_ instead which is NAN for market order.
        // For limit order, the global.limit_price_ should be set through SET_LIMIT brokerCommand before calling BrokerBuy2.
        auto [order, timed_out] = client_->sendOrder(Asset, nAmount > 0 ? Side::Buy : Side::Sell, std::abs(nAmount * global.amount_), global.limit_price_, global.order_duration_);
        
        // reset amount and limit price, they will be set again prior playing next order
        global.amount_ = 1.;
        global.limit_price_ = NAN;

        if (!order)
        {
            global.amount_ = 1.;
            global.limit_price_ = NAN;
            if (global.order_duration_ == (tsNCharcb)RApi::sORDER_DURATION_FOK || global.order_duration_ == (tsNCharcb)RApi::sORDER_DURATION_IOC)
            {
                SPDLOG_TRACE("BrokerBuy2 Order nullptr, return 1");
                return 1;
            }

            if (timed_out)
            {
                SPDLOG_TRACE("BrokerBuy2 Order nullptr, timedout. return -2");
                return -2;
            }
            SPDLOG_TRACE("BrokerBuy2 Order nullptr, return 0");
            return 0;
        }

        if (order->completion_reason_.pData && 
            (order->completion_reason_ == RApi::sCOMPLETION_REASON_FAILURE ||
            order->completion_reason_ == RApi::sCOMPLETION_REASON_CANCEL ||
            order->completion_reason_ == RApi::sCOMPLETION_REASON_REJECT))
        {
            SPDLOG_TRACE("BrokerBuy2 Order {}, return 0. complete reason {}", order->order_num_, to_string_view(order->completion_reason_));
            return 0;
        }

        if (order->exec_qty_)
        {
            if (pPrice)
            {
                *pPrice = order->avg_fill_price_;
            }
            if (pFill)
            {
                *pFill = static_cast<int>(order->exec_qty_);
            }
            SPDLOG_TRACE("BrokerBuy2 return Order {}, pFill: {}", order->order_num_, *pFill);
            return order->order_num_;
        }

        if (pFill)
        {
            *pFill = 0;
        }

        SPDLOG_TRACE("BrokerBuy2 return Order {} pFill: 0", order->order_num_);
        return order->order_num_;
    }

    DLLFUNC_C int BrokerTrade(int nTradeID, double* pOpen, double* pClose, double* pCost, double *pProfit)
    {
        SPDLOG_INFO("BrokerTrade: {}", nTradeID);
        auto order = client_->retrieveOrder(nTradeID);
        if (order)
        {
            if (order->cancelled_)
            {
                return NAY - 1;
            }

            if (order->exec_qty_)
            {
                if (pOpen)
                {
                    *pOpen = order->avg_fill_price_;
                }
            }

            if (order->exec_qty_ >= order->qty_)
            {
                auto position = client_->getPosition(order->symbol_.c_str());
                if (position.timestamp_)
                {
                    if ((order->side_ == Side::Buy && position.quantity_ <= 0) ||
                        (order->side_ == Side::Sell && position.quantity_ >= 0))
                    {
                        // the trade was completed closed
                        return -1;
                    }
                }
            }
            return order->exec_qty_;
        }
        BrokerError(std::format("Order {} not found", nTradeID).c_str());
        return NAY - 1;
    }

    // DLLFUNC_C int BrokerSell2(int nTradeID, int nAmount, double Limit, double* pClose, double* pCost, double* pProfit, int* pFill)
    // {
    //     SPDLOG_INFO("BrokerSell2: {} nAmount={}({}) Limit={}({}) duration={}", nTradeID, nAmount, global.amount_, Limit, global.limit_price_, to_string_view(global.order_duration_));
    //     return 0;
    // }

    DLLFUNC_C double BrokerCommand(int Command, intptr_t parameter)
    {
        switch (Command)
        {
        case GET_COMPLIANCE:
            return 15; // full NFA compliant

        case GET_BROKERZONE:
            return 0;   // historical data in UTC time

        case GET_MAXTICKS:
            return 10000;

        case GET_MAXREQUESTS:
            return 0;

        case GET_LOCK:
            return -1;

        case GET_POSITION: {
            global.last_position_ = client_->getPosition((char*)parameter);
            SPDLOG_DEBUG("GET_POSITION {}: {}@{}", (char*)parameter, global.last_position_.quantity_, global.last_position_.average_price_);
            return global.last_position_.quantity_;
        }

        case GET_AVGENTRY:
            return global.last_position_.average_price_;

        case SET_ORDERTEXT:
            global.order_text_ = (char*)parameter;
            SPDLOG_TRACE("SET_ORDERTEXT: {}", global.order_text_);
            return parameter;

        case SET_SYMBOL:
            global.symbol_ = (char*)parameter;
            SPDLOG_TRACE("SET_SYMBOL: {}", global.symbol_);
            return parameter;

        case SET_MULTIPLIER:
            global.multiplier_ = (double)parameter;
            SPDLOG_TRACE("SET_MULTIPLIER: {}", global.multiplier_);
            return parameter;

        case SET_ORDERTYPE: {
            switch ((int)parameter)
            {
            case 0:
                global.order_duration_ = RApi::sORDER_DURATION_IOC;
                break;
            case 1:
                global.order_duration_ = RApi::sORDER_DURATION_FOK;
                break;
            case 2:
                global.order_duration_ = RApi::sORDER_DURATION_GTC;
                break;
            default:
                SPDLOG_INFO("SET_ORDERTYPE: {} unsupported. Use default IOC.", (int)parameter);
                global.order_duration_ = RApi::sORDER_DURATION_IOC;
                break;
            }
            SPDLOG_TRACE("SET_ORDERTYPE: {}", to_string(global.order_duration_));
            return parameter;
        }

        case SET_WAIT:
            global.wait_time_ = (int)parameter * 1000000;
            SPDLOG_TRACE("SET_WAIT: {} ns", global.wait_time_);
            return parameter;

        case GET_PRICETYPE:
            return global.price_type_.load(std::memory_order_relaxed);

        case SET_PRICETYPE:
        {
            global.price_type_.store((int)parameter, std::memory_order_release);
            SPDLOG_TRACE("SET_PRICETYPE: {}", (int)parameter);
            return parameter;
        }

        case SET_AMOUNT:
            global.amount_ = *(double*)parameter;
            SPDLOG_TRACE("SET_AMOUNT: {}", global.amount_);
            return parameter;

        case SET_DIAGNOSTICS:
            if ((int)parameter == 1)
            {
                spdlog::set_level(spdlog::level::trace);
            }
            else if ((int)parameter == 0)
            {
                spdlog::set_level((spdlog::level::level_enum)config.log_level_);
            }
            return parameter;

        case SET_LIMIT:
            global.limit_price_ = *(double*)parameter;
            SPDLOG_TRACE("SET_LIMIT: {}", global.limit_price_);
            return parameter;

        case GET_OPTIONS:
            SPDLOG_TRACE("GET_OPTIONS: {}", (char*)parameter);
            return 0;

        case GET_FUTURES: {
            SPDLOG_TRACE("GET_FUTURES: {}", global.symbol_);
            return 0;
        }

        case DO_CANCEL:
        {
            auto order_id = (int)parameter;
            client_->cancelOrder(order_id);
            return parameter;
        }

        case 2000:
        {
            auto level = (int)parameter;
            if (level < SPDLOG_LEVEL_TRACE || level > SPDLOG_LEVEL_OFF)
            {
                return 0;
            }

            SPDLOG_TRACE("Set log level: {}", level);
            spdlog::set_level((spdlog::level::level_enum)level);
            if (level > SPDLOG_LEVEL_INFO)
            {
                spdlog::flush_on((spdlog::level::level_enum)level);
            }
            return parameter;
        }

        case 2001:
        {
            if ((int)parameter)
            {
                global.order_duration_ = RApi::sORDER_DURATION_DAY;
                SPDLOG_TRACE("Set default order type to Day");
            }
            else
            {
                global.order_duration_ = RApi::sORDER_DURATION_IOC;
                SPDLOG_TRACE("Set default order type to IOC");
            }
            return parameter;
        }

        case GET_VOLTYPE:
        {
            auto rt = global.price_type_.load(std::memory_order_relaxed) == 2 ? 4 : 3;
            SPDLOG_TRACE("GET_VOLTYPE: {}", rt);
            return rt;
        }

        case SET_VOLTYPE:
            SPDLOG_TRACE("SET_VOLTYPE: {}", (int)parameter);
            global.vol_type_ = (int)parameter;
            return parameter;

        case SET_HWND:
        {
            SPDLOG_TRACE("SET_HWND: 0x{:x}", parameter);
            global.handle_ = (HWND)parameter;
            return parameter;
        }

        case SET_FUNCTIONS:
        {
            FARPROC* Functions = (FARPROC*)parameter;
            (FARPROC &)report = Functions[565];
            break;
        }

        case GET_HEARTBEAT:
        case GET_CALLBACK:
        case SET_CCY:
        case SET_LEVERAGE:
            return 0;

        default:
            SPDLOG_DEBUG("Unhandled command: {} {}", Command, parameter);
            break;
        }
        return 0;
    }
}
