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

#pragma once

#include <RApiPlus.h>
#include <cstdint>
#include <memory>
#include <atomic>
#include <string>
#include <string_view>
#include <unordered_map>
#include <chrono>
#include "symbol.h"
#include "Order.h"
#include "pnl.h"
#include "rithmic_system_config.h"

#include <windows.h>
typedef double DATE;			//prerequisite for using trading.h
#include <include\trading.h>	// enter your path to trading.h (in your Zorro folder)

#ifdef SPDLOG_ACTIVE_LEVEL
#undef SPDLOG_ACTIVE_LEVEL
#endif
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE

namespace zorro {

extern int(__cdecl* BrokerError)(const char* txt);
extern int(__cdecl* BrokerProgress)(const int percent);

struct AdmCallbacks : public RApi::AdmCallbacks
{
    AdmCallbacks() {};
    ~AdmCallbacks() override = default;

    int Alert(RApi::AlertInfo *pInfo, void *pContext, int *aiCode) override;
};

class RithmicClient : public RApi::RCallbacks
{
    static constexpr uint32_t MAX_ORDER_NUM = 1000000;

    RithmicSystemConfig system_config_;

    const uint64_t pid_;
    std::string user_;
    std::string env_user_;
    std::string password_;
    AdmCallbacks adm_callbacks_;
    RApi::REngineParams engine_params_;
    std::unique_ptr<RApi::REngine> engine_;

    std::string fcm_id_;
    std::string ib_id_;
    std::string account_id_;
    std::string account_name_;
    RApi::AccountInfo account_info_;
    std::unordered_map<std::string, std::string> trade_routes_;

    enum LoginStatus : uint8_t
    {
        MarketDataComplete,
        TradingSystemComplete,
        IntradayHistoryComplete,
        PnLComplete,
        FailureOccured,
        __Count__,
    };
    std::atomic<std::bitset<LoginStatus::__Count__>> login_status_;

    enum class RequestStatus : uint8_t
    {
        NoRequest,
        AwaitingResults,
        Complete,
        Failed,
        Timeout,
    };

    std::atomic_bool unaccepted_agreements_received_;
    std::atomic_bool account_received_;
    std::atomic<RequestStatus> request_status_{RequestStatus::NoRequest};
    std::atomic<uint64_t> pending_order_request_;
    bool has_unaccepted_aggreements_;

    std::unordered_map<std::string, Symbol> symbols_;
    std::unordered_map<uint32_t, std::atomic<std::shared_ptr<Order>>*> orders_by_id_;
    std::array<std::atomic<std::shared_ptr<Order>>, MAX_ORDER_NUM> orders_;
    std::atomic_uint_fast32_t next_order_index_;
    
    std::vector<T6> ticks_;
    size_t n_ticks_requested_ = 0;

    std::atomic<PnL> pnl_;
    std::unordered_map<std::string, std::atomic<Position>> asset_position_;

public:
    RithmicClient(std::string user);
    ~RithmicClient();

    const std::vector<std::string>& getServerNames() const noexcept { return system_config_.server_names; }

    void setServer(const std::string &server);

    std::string_view accountId() const noexcept;
    PnL pnl() const noexcept { return pnl_.load(std::memory_order_relaxed); }

    bool login(std::string password, std::string &err);
    bool getRefData(tsNCharcb &exchange, tsNCharcb &symbol);
    bool getPriceIncInfo(tsNCharcb &exchange, tsNCharcb &symbol);
    bool subscribe(const char* asset);
    Symbol* getSymbol(const char* asset);
    Symbol* getSymbol(const std::string &exchange, const std::string &ticker);

    const std::vector<T6>& replayBars(char* asset, int64_t start, int64_t end, int n_tick_minutes, int n_ticks);

    Position getPosition(const char* asset) const;

    /**
     * @brief Send an order to the exchange
     * @param asset The asset to trade
     * @param side The side of the order
     * @param quantity The quantity of the order
     * @param price The price of the order. If NAN, the order will be a market order. Default is NAN
     * @param duration The duration of the order. Default is Day
     * @param trigger_price The trigger price of the stop order. Default is NAN. If not NAN, the order will be a stop order.
     * @param is_short If true, the order will be a short order. Default is false.
     * @return std::pair<std::shared_ptr<Order>, bool>. First: the order object, nullptr if the order was not sent or an error occurred. Second: is time out
     */
    std::pair<std::shared_ptr<Order>, bool> sendOrder(const char* asset, Side side, int quantity, double price = NAN, const tsNCharcb &duration = RApi::sORDER_DURATION_DAY, double trigger_price = NAN, bool is_short = false);

    std::shared_ptr<Order> getOrder(uint32_t order_id) const;

    std::shared_ptr<Order> retrieveOrder(uint32_t order_id);

    bool cancelOrder(uint32_t order_id);

    std::vector<Spec> searchInstrument(const std::string &search);

public:
    // RApi callbacks
    int Alert(RApi::AlertInfo *pInfo, void *pContext, int *aiCode) override;
    int AgreementList(RApi::AgreementListInfo *pInfo, void *pContext, int *aiCode) override;
    int AccountList(RApi::AccountListInfo *pInfo, void *pContext, int *aiCode) override;
    int TradeRouteList(RApi::TradeRouteListInfo * pInfo, void * pContext, int * aiCode) override;

    // MD callbacks
    int RefData(RApi::RefDataInfo *pInfo, void *pContext, int * aiCode) override;
    int PriceIncrUpdate(RApi::PriceIncrInfo *pInfo, void *pContext, int *aiCode) override;
    int BestAskQuote(RApi::AskInfo *pInfo, void *pContext, int *aiCode) override;
    int BestBidAskQuote(RApi::BidInfo *pBid, RApi::AskInfo *pAsk, void *pContext, int *aiCode) override;
    int BestBidQuote(RApi::BidInfo *pInfo, void *pContext, int *aiCode) override;
    int MarketMode(RApi::MarketModeInfo *pInfo, void *pContext, int *aiCode) override;

    // Order callbacks
    int OpenOrderReplay(RApi::OrderReplayInfo *pInfo, void *pContext, int *aiCode) override;
    int LineUpdate(RApi::LineInfo * pInfo, void * pContext, int * aiCode) override;
    int BustReport(RApi::OrderBustReport * pReport, void * pContext, int * aiCode) override;
    int CancelReport(RApi::OrderCancelReport * pReport, void * pContext, int * aiCode) override;
    int FailureReport(RApi::OrderFailureReport * pReport, void * pContext, int * aiCode) override;
    int FillReport(RApi::OrderFillReport * pReport, void * pContext, int * aiCode) override;
    int ModifyReport(RApi::OrderModifyReport * pReport, void * pContext, int * aiCode) override;
    int NotCancelledReport(RApi::OrderNotCancelledReport * pReport, void * pContext, int * aiCode);
    int NotModifiedReport(RApi::OrderNotModifiedReport * pReport, void * pContext, int * aiCode);
    int RejectReport(RApi::OrderRejectReport * pReport, void * pContext, int * aiCode);
    int StatusReport(RApi::OrderStatusReport * pReport, void * pContext, int * aiCode);
    int TriggerReport(RApi::OrderTriggerReport * pReport, void * pContext, int * aiCode);
    int OtherReport(RApi::OrderReport * pReport, void * pContext, int * aiCode) override;
    int SingleOrderReplay(RApi::SingleOrderReplayInfo *pInfo, void *pContext, int *aiCode);

    // Historical callbacks
    int Bar(RApi::BarInfo *pInfo, void *pContext, int *aiCode) override;
    int BarReplay(RApi::BarReplayInfo *pInfo, void *pContext, int *aiCode) override;

    // PnL callbacks
    int PnlReplay(RApi::PnlReplayInfo *pInfo, void *pContext, int *aiCode) override;
    int PnlUpdate(RApi::PnlInfo *pInfo, void *pContext, int *aiCode) override;

    // Trade callbacks
    int TradePrint(RApi::TradeInfo *pInfo, void *pContext, int *aiCode) override;

private:
    bool checkAgreements(std::string &err);
    RequestStatus waitForRequest(uint32_t timeout_ms = 0);
    void setMDReady(Symbol &symbol, MDReady falg);
    bool listTradeRoutes();
    bool subscribeOrder();
    bool subscribePnl();
    void handlePnlInfo(const RApi::PnlInfo &pnl_info);
    std::pair<std::shared_ptr<Order>, bool> sendLimitOrder(Symbol *symbol, Side side, int quantity, double price, const tsNCharcb &duration, const std::string &trade_route, bool is_short = false);
    std::pair<std::shared_ptr<Order>, bool> sendMarketOrder(Symbol *symbol, Side side, int quantity, const std::string &trade_route, bool is_short = false);
    std::pair<std::shared_ptr<Order>, bool> sendStopLimitOrder(Symbol *symbol, Side side, int quantity, double price, double trigger_price, const tsNCharcb &duration, const std::string &trade_route, bool is_short = false);
    std::pair<std::shared_ptr<Order>, bool> sendStopMarketOrder(Symbol *symbol, Side side, int quantity, double trigger_price, const std::string &trade_route, bool is_short = false);

    template<typename ParamsT>
    std::pair<std::shared_ptr<Order>, bool> doSendOrder(ParamsT &params, Side side, double price, int qty);
};

}