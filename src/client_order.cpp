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
    auto &global = zorro::Global::get();
}

std::shared_ptr<Order> RithmicClient::getOrder(uint32_t order_id) const
{
    auto iter = orders_by_id_.find(order_id);
    if (iter == orders_by_id_.end())
    {
        return nullptr;
    }
    return iter->second->load(std::memory_order_relaxed);
}

std::shared_ptr<Order> RithmicClient::retrieveOrder(uint32_t order_id)
{
    auto order = getOrder(order_id);
    if (order)
    {
        return order;
    }

    order = std::make_shared<Order>();
    order->client_order_id_ = 0;
    order->order_num_ = order_id;
    order->str_order_num_ = std::to_string(order_id);

    auto order_index = next_order_index_.fetch_add(1, std::memory_order_relaxed);

    auto &atomic_order = orders_[order_index];
    atomic_order.store(order, std::memory_order_relaxed);

    tsNCharcb order_num {order->str_order_num_.data(), (int)order->str_order_num_.size()};

    int iCode;
    if (!engine_->setOrderContext(&order_num, &atomic_order, &iCode))
    {
        BrokerError(std::format("Failed to set OrderContext. orderNum={} err: {}", order_id, iCode).c_str());
        return nullptr;
    }

    pending_order_request_.store(order_id, std::memory_order_relaxed);
    if (!engine_->replaySingleOrder(&account_info_, &order_num, &atomic_order, &iCode))
    {
        BrokerError(std::format("REngine::replaySingleOrder() err: {}", iCode).c_str());
        return nullptr;
    }

    while (pending_order_request_.load(std::memory_order_relaxed))
    {
        if (!BrokerProgress(1))
        {
            return nullptr;
        }
    }

    auto retrieved_order = atomic_order.load(std::memory_order_relaxed);
    if (retrieved_order->client_order_id_ == 0)
    {
        return nullptr;
    }
    orders_by_id_[retrieved_order->order_num_] = &atomic_order;
    return retrieved_order;
}

bool RithmicClient::subscribeOrder()
{
    request_status_.store(RequestStatus::AwaitingResults, std::memory_order_relaxed);
    int iCode;
    if (!engine_->subscribeOrder(&account_info_, &iCode))
    {
        BrokerError(std::format("REgine::subscribeOrder() err: {}", iCode).c_str());
        return false;
    }

    if (!engine_->replayOpenOrders(&account_info_, &iCode))
    {
        BrokerError(std::format("REgine::replayOpenOrderss() err: {}", iCode).c_str());
        return false;
    }

    auto status = waitForRequest();
    return status == RequestStatus::Complete;
}

int RithmicClient::OpenOrderReplay(OrderReplayInfo *pInfo, void *pContext, int *aiCode)
{
    if (pInfo->iRpCode == API_OK)
    {
        int iCode;
        for (auto i = 0; i < pInfo->iArrayLen; ++i)
        {
            auto &line_info = pInfo->asLineInfoArray[i];
            auto userTag = to_string_view(line_info.sUserTag);
            if (userTag.substr(0, 6) != "ZORRO_")
            {
                // order placed by other application, try next order
                continue;
            }

            auto order_index = next_order_index_.fetch_add(1, std::memory_order_relaxed);
            uint32_t client_order_id = atoi(userTag.substr(6).data());

            auto order = std::make_shared<Order>();
            order->client_order_id_ = client_order_id;
            order->exchange_ = to_string_view(line_info.sExchange);
            order->ticker_ = to_string_view(line_info.sTicker);
            order->symbol_ = symbol(&line_info);
            order->exch_ord_id_ = to_string_view(line_info.sExchOrdId);
            order->ticker_plant_exch_ord_id_ = to_string_view(line_info.sTickerPlantExchOrdId);

            order->side_ = strcmp(line_info.sBuySellType.pData, sBUY_SELL_TYPE_BUY.pData) == 0 ? Side::Buy : Side::Sell;

            if (line_info.bPriceToFillFlag)
            {
                order->avg_fill_price_ = line_info.dPriceToFill;
            }

            if (line_info.bAvgFillPriceFlag)
            {
                order->avg_fill_price_ = line_info.dAvgFillPrice;
            }

            if (line_info.bTriggerPriceFlag)
            {
                order->trigger_price_ = line_info.dTriggerPrice;
            }

            order->qty_ = line_info.llQuantityToFill;
            order->exec_qty_ = line_info.llFilled;
            order->str_order_num_ = to_string_view(line_info.sOrderNum);
            order->order_num_ = atoi(to_string_view(line_info.sOrderNum).data());
            order->original_order_num_ = to_string_view(line_info.sOriginalOrderNum);
            order->initial_sequence_number_ = to_string_view(line_info.sInitialSequenceNumber);
            order->current_sequence_number_ = to_string_view(line_info.sCurrentSequenceNumber);
            order->omni_bus_account_ = to_string_view(line_info.sOmnibusAccount);
            order->order_type_ = line_info.sOrderType;
            order->original_order_type_ = line_info.sOriginalOrderType;
            order->duration_ = line_info.sOrderDuration;
            order->trade_route_ = line_info.sTradeRoute;

            orders_[order_index].store(order, std::memory_order_release);   // store the order

            if (!engine_->setOrderContext(&line_info.sOrderNum, &orders_[order_index], &iCode))
            {
                SPDLOG_ERROR("REngine::setOrderContext() failed. orderNum: {} order_index: {} err: {}", order->order_num_, order_index, iCode);
            }
        }
        request_status_.store(RequestStatus::Complete, std::memory_order_release);
    }
    else
    {
        SPDLOG_DEBUG(to_string_view(pInfo->sRpCode));
        request_status_.store(RequestStatus::Failed, std::memory_order_release);
    }
    *aiCode = API_OK;
    return (OK);
}

std::pair<std::shared_ptr<Order>, bool> RithmicClient::sendOrder(const char* asset, Side side, int quantity, double price, const tsNCharcb &duration, double trigger_price, bool is_short)
{
    auto *symbol = getSymbol(asset);
    if (!symbol)
    {
        BrokerError(std::format("Symbol {} not found", asset).c_str());
        return std::make_pair(nullptr, false);
    }

    if (!symbol->can_trade_.load(std::memory_order_relaxed))
    {
        BrokerError(std::format("{} is closed", asset).c_str());
        return std::make_pair(nullptr, false);
    }

    auto iter_trade_route = trade_routes_.find(symbol->spec_.exchange_);
    if (iter_trade_route == trade_routes_.end())
    {
        BrokerError(std::format("Trade route for exchange {} not found", symbol->spec_.exchange_).c_str());
        return std::make_pair(nullptr, false);
    }

    if (std::isnan(price))
    {
        if (std::isnan(trigger_price))
        {
            return sendMarketOrder(symbol, side, quantity, iter_trade_route->second, is_short);
        }
        return sendStopMarketOrder(symbol, side, quantity, trigger_price, iter_trade_route->second, is_short);
    }

    if (std::isnan(trigger_price))
    {
        return sendLimitOrder(symbol, side, quantity, price, duration, iter_trade_route->second, is_short);
    }
    return sendStopLimitOrder(symbol, side, quantity, price, trigger_price, duration, iter_trade_route->second, is_short);
}

std::pair<std::shared_ptr<Order>, bool> RithmicClient::sendLimitOrder(Symbol *symbol, Side side, int quantity, double price, const tsNCharcb &duration, const std::string &trade_route, bool is_short)
{
    LimitOrderParams params;
    params.dPrice = price;
    params.iQty = quantity;
    params.pAccount = &account_info_;
    params.sBuySellType = side == Side::Buy ? sBUY_SELL_TYPE_BUY : (is_short ? sBUY_SELL_TYPE_SELL_SHORT : sBUY_SELL_TYPE_SELL);
    params.sEntryType = sORDER_ENTRY_TYPE_AUTO;
    params.sExchange.pData = symbol->spec_.exchange_.data();
    params.sExchange.iDataLen = static_cast<int>(symbol->spec_.exchange_.length());
    params.sTicker.pData = symbol->spec_.ticker_.data();
    params.sTicker.iDataLen = static_cast<int>(symbol->spec_.ticker_.length());
    params.sTradeRoute.pData = (char*)trade_route.data();
    params.sTradeRoute.iDataLen = static_cast<int>(trade_route.length());
    params.sDuration = duration;
    params.dPrice = price;
    params.iQty = quantity;
    return doSendOrder(params, side, price, quantity);
}

std::pair<std::shared_ptr<Order>, bool> RithmicClient::sendMarketOrder(Symbol *symbol, Side side, int quantity, const std::string &trade_route, bool is_short)
{
    MarketOrderParams params;
    params.pAccount = &account_info_;
    params.sBuySellType = side == Side::Buy ? sBUY_SELL_TYPE_BUY : (is_short ? sBUY_SELL_TYPE_SELL_SHORT : sBUY_SELL_TYPE_SELL);
    params.sExchange.pData = symbol->spec_.exchange_.data();
    params.sExchange.iDataLen = static_cast<int>(symbol->spec_.exchange_.length());
    params.sTicker.pData = symbol->spec_.ticker_.data();
    params.sTicker.iDataLen = static_cast<int>(symbol->spec_.ticker_.length());
    params.sTradeRoute.pData = (char*)trade_route.data();
    params.sTradeRoute.iDataLen = static_cast<int>(trade_route.length());
    params.sDuration = sORDER_DURATION_DAY;
    params.sEntryType = sORDER_ENTRY_TYPE_AUTO;
    params.iQty = quantity;
    return doSendOrder(params, side, NAN, quantity);
}

std::pair<std::shared_ptr<Order>, bool> RithmicClient::sendStopLimitOrder(Symbol *symbol, Side side, int quantity, double price, double trigger_price, const tsNCharcb &duration, const std::string &trade_route, bool is_short)
{
    StopLimitOrderParams params;
    params.pAccount = &account_info_;
    params.sBuySellType = side == Side::Buy ? sBUY_SELL_TYPE_BUY : (is_short ? sBUY_SELL_TYPE_SELL_SHORT : sBUY_SELL_TYPE_SELL);
    params.sExchange.pData = symbol->spec_.exchange_.data();
    params.sExchange.iDataLen = static_cast<int>(symbol->spec_.exchange_.length());
    params.sTicker.pData = symbol->spec_.ticker_.data();
    params.sTicker.iDataLen = static_cast<int>(symbol->spec_.ticker_.length());
    params.sTradeRoute.pData = (char*)trade_route.data();
    params.sTradeRoute.iDataLen = static_cast<int>(trade_route.length());
    params.sEntryType = sORDER_ENTRY_TYPE_AUTO;
    params.sDuration = duration;
    params.dPrice = price;
    params.dTriggerPrice = trigger_price;
    params.iQty = quantity;
    return doSendOrder(params, side, price, quantity);
}

std::pair<std::shared_ptr<Order>, bool> RithmicClient::sendStopMarketOrder(Symbol *symbol, Side side, int quantity, double trigger_price, const std::string &trade_route, bool is_short)
{
    StopMarketOrderParams params;
    params.pAccount = &account_info_;
    params.sBuySellType = side == Side::Buy ? sBUY_SELL_TYPE_BUY : (is_short ? sBUY_SELL_TYPE_SELL_SHORT : sBUY_SELL_TYPE_SELL);
    params.sExchange.pData = symbol->spec_.exchange_.data();
    params.sExchange.iDataLen = static_cast<int>(symbol->spec_.exchange_.length());
    params.sTicker.pData = symbol->spec_.ticker_.data();
    params.sTicker.iDataLen = static_cast<int>(symbol->spec_.ticker_.length());
    params.sTradeRoute.pData = (char*)trade_route.data();
    params.sTradeRoute.iDataLen = static_cast<int>(trade_route.length());
    params.sDuration = sORDER_DURATION_DAY;
    params.sEntryType = sORDER_ENTRY_TYPE_AUTO;
    params.dTriggerPrice = trigger_price;
    params.iQty = quantity;
    return doSendOrder(params, side, NAN, quantity);
}

template<typename ParamT>
std::pair<std::shared_ptr<Order>, bool> RithmicClient::doSendOrder(ParamT &params, Side side, double price, int qty)
{
    auto order = std::make_shared<Order>();
    order->side_ = side;
    order->price_ = price;
    order->qty_ = qty;
    order->exchange_ = to_string_view(params.sExchange);
    order->ticker_ = to_string_view(params.sTicker);
    order->symbol_ = symbol(&params);

    auto order_index = next_order_index_.fetch_add(1, std::memory_order_relaxed);
    auto client_order_id = pid_ << 32 | order_index;
    order->client_order_id_ = client_order_id;
    order->tag_ = std::format("ZORRO_{}", client_order_id);
    order->duration_ = params.sDuration;

    auto &atomic_order = orders_[order_index];
    atomic_order.store(order, std::memory_order_relaxed);

    params.sTag.pData = order->tag_.data();
    params.sTag.iDataLen = static_cast<int>(order->tag_.length());
    params.pContext = &atomic_order;

    if (!global.order_text_.empty())
    {
        params.sUserMsg.pData = global.order_text_.data();
        params.sUserMsg.iDataLen = static_cast<int>(global.order_text_.length());
    }

    SPDLOG_DEBUG("Send order. ZORRO_{} side={} price={} qty={} duration={}", client_order_id, (int)side, price, qty, to_string_view(params.sDuration));
    pending_order_request_.store(client_order_id, std::memory_order_release);
    int iCode;
    if (!engine_->sendOrder(&params, &iCode))
    {
        BrokerError(std::format("REngine::sendOrder() err: {}", iCode).c_str());
        return std::make_pair(nullptr, false);
    }

    auto start = get_timestamp();
    while (pending_order_request_.load(std::memory_order_relaxed))
    {
        if (!BrokerProgress(1))
        {
            SPDLOG_DEBUG("BrokerProgress failed");
            return std::make_pair(nullptr, false);
        }

        if ((get_timestamp() - start) >= global.wait_time_)
        {
            if (!std::isnan(price))
            {
                // limit order
                auto ord = atomic_order.load(std::memory_order_relaxed);
                std::shared_ptr<Order> new_order;
                do
                {
                    if (ord->order_num_)
                    {
                        cancelOrder(ord->order_num_);
                        break;
                    }
                    new_order = std::make_shared<Order>(*ord.get());
                    new_order->pending_cancel_ = true;
                }
                while (!atomic_order.compare_exchange_weak(ord, new_order, std::memory_order_release, std::memory_order_relaxed));
            }
            pending_order_request_.store(0, std::memory_order_relaxed);
            return std::make_pair(nullptr, true);
        }
    }

    auto updated_order = atomic_order.load(std::memory_order_relaxed);
    if (!updated_order->text_.empty())
    {
        BrokerError(updated_order->text_.c_str());
    }

    if (updated_order->order_num_ == 0)
    {
        return std::make_pair(nullptr, false);
    }

    if (updated_order->completion_reason_.pData)
    {
        if (updated_order->completion_reason_ != (tsNCharcb)sCOMPLETION_REASON_FILL &&
            updated_order->completion_reason_ != (tsNCharcb)sCOMPLETION_REASON_PFBC)
        {
            // Order cancelled, rejected or failed
            return std::make_pair(nullptr, false);
        }
    }
    orders_by_id_[updated_order->order_num_] = &atomic_order;
    return std::make_pair(updated_order, false);
}

int RithmicClient::LineUpdate(LineInfo *pInfo, void *pContext, int *aiCode)
{
    auto tag = to_string_view(pInfo->sTag);
    if (tag.substr(0, 6) != "ZORRO_")
    {
        // order placed by other application
        *aiCode = API_OK;
        return OK;
    }

    uint64_t client_order_id = atoll(tag.substr(6).data());
    if (pInfo->iRpCode == API_OK)
    {
        auto order_timestamp = nanosec(*pInfo);
        SPDLOG_DEBUG("LineUpdate: {} {} conext={:x} orderNum={} exch_id={} type={} tickerPlantExchOrdId={} timestamp={} {} {} {} releaseCommand={} side={} order_type={} price={} qty={} filled={} status={} sCompletionReason={} remark={} text={} sUserTag={} sUserMsg={}",
            to_string_view(pInfo->sTicker), to_string_view(pInfo->sTag), reinterpret_cast<uintptr_t>(pInfo->pContext), to_string_view(pInfo->sOrderNum), to_string_view(pInfo->sExchOrdId), pInfo->iType,
            to_string_view(pInfo->sTickerPlantExchOrdId), pInfo->iSsboe, pInfo->iUsecs, pInfo->iCancelSsboe, pInfo->iCancelUsecs, to_string_view(pInfo->sReleaseCommand), to_string_view(pInfo->sBuySellType), to_string_view(pInfo->sOrderType), pInfo->dPriceToFill, pInfo->llQuantityToFill, pInfo->llFilled,
            to_string_view(pInfo->sStatus), to_string_view(pInfo->sCompletionReason), to_string_view(pInfo->sRemarks), to_string_view(pInfo->sText),
            to_string_view(pInfo->sUserTag), to_string_view(pInfo->sUserMsg));

        auto *atomic_order = static_cast<std::atomic<std::shared_ptr<Order>>*>(pInfo->pContext);
        if (!atomic_order || atomic_order < &orders_.front() || atomic_order > &orders_.back())
        {
            // order placed by other application
            *aiCode = API_OK;
            return OK;
        }

        if (!pInfo->llQuantityToFill)
        {
            // intermediate status update
            *aiCode = API_OK;
            return OK;
        }

        auto order = atomic_order->load(std::memory_order_relaxed);
        std::shared_ptr<Order> updated_order;
        auto order_num = atoi(to_string_view(pInfo->sOrderNum).data());
        if (pInfo->iType == RApi::MD_HISTORY_CB)
        {
            // from replay
            assert(order->order_num_ == order_num);
        }
        else
        {
            if (order->client_order_id_ != client_order_id)
            {
                SPDLOG_ERROR("ClientOrderId mismatch: {} {} orderNum: {} client_order_id: {} conext client_order_id: {}", to_string_view(pInfo->sTicker), to_string_view(pInfo->sTag), to_string_view(pInfo->sOrderNum),
                    client_order_id, order->client_order_id_);
                *aiCode = API_OK;
                return OK;
            }
        }

        do
        {
            if(order->last_update_time_ > order_timestamp)
            {
                // stale information
                break;
            }

            if (order->pending_cancel_)
            {
                int iCode;
                if (!engine_->cancelOrder(&account_info_, &pInfo->sOrderNum, (tsNCharcb*)&sORDER_ENTRY_TYPE_AUTO, nullptr, nullptr, nullptr, &iCode))
                {
                    auto msg = std::format("Pending cancel, failed to cancel. {} {} {}. err: {}", to_string_view(pInfo->sTicker), to_string_view(pInfo->sTag), order_num, iCode);
                    SPDLOG_ERROR(msg);
                    BrokerError(msg.c_str());
                }
                order->pending_cancel_ = false;
                *aiCode = API_OK;
                return OK;
            }

            updated_order = std::make_shared<Order>(*order.get());

            if (pInfo->iType == RApi::MD_HISTORY_CB)
            {
                if (updated_order->tag_.empty())
                {
                    updated_order->exchange_ = to_string_view(pInfo->sExchange);
                    updated_order->ticker_ = to_string_view(pInfo->sTicker);
                    updated_order->symbol_ = symbol(pInfo);
                    updated_order->tag_ = to_string_view(pInfo->sTag);
                    updated_order->client_order_id_ = client_order_id;
                }
            }
            else
            {
                updated_order->str_order_num_ = to_string_view(pInfo->sOrderNum);
                updated_order->order_num_ = order_num;
            }

            if (pInfo->bPriceToFillFlag)
            {
                updated_order->price_ = pInfo->dPriceToFill;
            }

            if (pInfo->bAvgFillPriceFlag)
            {
                updated_order->avg_fill_price_ = pInfo->dAvgFillPrice;
            }

            if (pInfo->bTriggerPriceFlag)
            {
                updated_order->trigger_price_ = pInfo->dTriggerPrice;
            }

            updated_order->qty_ = pInfo->llQuantityToFill;
            updated_order->exec_qty_ = pInfo->llFilled;
            updated_order->original_order_num_ = to_string_view(pInfo->sOriginalOrderNum);
            updated_order->initial_sequence_number_ = to_string_view(pInfo->sInitialSequenceNumber);
            updated_order->current_sequence_number_ = to_string_view(pInfo->sCurrentSequenceNumber);
            updated_order->omni_bus_account_ = to_string_view(pInfo->sOmnibusAccount);
            updated_order->order_type_ = pInfo->sOrderType;
            updated_order->original_order_type_ = pInfo->sOriginalOrderType;
            updated_order->trade_route_ = pInfo->sTradeRoute;
            updated_order->exch_ord_id_ = to_string_view(pInfo->sExchOrdId);
            updated_order->ticker_plant_exch_ord_id_ = to_string_view(pInfo->sTickerPlantExchOrdId);
            updated_order->last_update_time_ = order_timestamp;
            updated_order->status_, pInfo->sStatus;
            updated_order->completion_reason_ = pInfo->sCompletionReason;
        } while (!atomic_order->compare_exchange_weak(order, updated_order, std::memory_order_release, std::memory_order_relaxed));
        
        if (pInfo->iType != RApi::MD_HISTORY_CB &&
            ((pInfo->sCompletionReason.pData && pInfo->sCompletionReason.iDataLen) /*completed*/ || 
             (updated_order && (updated_order->duration_ == (tsNCharcb)sORDER_DURATION_DAY || updated_order->duration_ == (tsNCharcb)sORDER_DURATION_GTC) &&
               pInfo->sOrderType != sORDER_TYPE_MARKET && pInfo->sOrderType != sORDER_TYPE_STOP_MARKET && pInfo->sStatus == (tsNCharcb)sLINE_STATUS_OPEN)) &&
            pending_order_request_.load(std::memory_order_relaxed) == client_order_id)
        {
            SPDLOG_DEBUG("LineUpdate: Reset pending order request");
            pending_order_request_.store(0, std::memory_order_relaxed);
        }
    }
    else
    {
        SPDLOG_DEBUG(to_string_view(pInfo->sRpCode));
        if (pInfo->iType != RApi::MD_HISTORY_CB && pending_order_request_.load(std::memory_order_relaxed) == client_order_id)
        {
            pending_order_request_.store(0, std::memory_order_relaxed);
        }
    }
    *aiCode = API_OK;
    return OK;
}

int RithmicClient::SingleOrderReplay(RApi::SingleOrderReplayInfo *pInfo, void *pContext, int *aiCode)
{
    auto order_num = atoi(to_string_view(pInfo->sOrderNum).data());
    if (pending_order_request_.load(std::memory_order_relaxed) == order_num)
    {
        pending_order_request_.store(0, std::memory_order_release);
    }
    else
    {
        assert(false);
    }
    *aiCode = API_OK;
    return OK;
}

int RithmicClient::BustReport(RApi::OrderBustReport * pReport, void * pContext, int * aiCode)
{
    SPDLOG_DEBUG("BustReport: {} {} order_num={} exch_ord_id={}", to_string_view(pReport->sTicker), to_string_view(pReport->sTag), to_string_view(pReport->sOrderNum), to_string_view(pReport->sExchOrdId));
    *aiCode = API_OK;
    return OK;
}

int RithmicClient::CancelReport(RApi::OrderCancelReport * pReport, void * pContext, int * aiCode)
{
    auto tag = to_string_view(pReport->sTag);
    if (tag.substr(0, 6) != "ZORRO_" || pReport->iType == MD_HISTORY_CB)
    {
        // order placed by other application
        *aiCode = API_OK;
        return OK;
    }

    uint64_t client_order_id = atoll(tag.substr(6).data());
    SPDLOG_INFO("CancelReport: {} {} order_num={} context={:x} exch_ord_id={}", to_string_view(pReport->sTicker), to_string_view(pReport->sTag),
        to_string_view(pReport->sOrderNum), reinterpret_cast<uintptr_t>(pReport->pContext), to_string_view(pReport->sExchOrdId));

    auto *atomic_order = static_cast<std::atomic<std::shared_ptr<Order>>*>(pReport->pContext);
    if (pReport->iType == RApi::MD_HISTORY_CB || !atomic_order || atomic_order < &orders_.front() || atomic_order > &orders_.back())
    {
        // order from history or placed by other application
        *aiCode = API_OK;
        return OK;
    }

    auto order = atomic_order->load(std::memory_order_relaxed);
    if (order->client_order_id_ != client_order_id)
    {
        SPDLOG_ERROR("ClientOrderId mismatch: {} {} orderNum: {} client_order_id: {} conext client_order_id: {}", to_string_view(pReport->sTicker), to_string_view(pReport->sTag), to_string_view(pReport->sOrderNum),
            client_order_id, order->client_order_id_);
        *aiCode = API_OK;
        return OK;
    }

    std::shared_ptr<Order> updated_order;
    auto order_num = atoi(to_string_view(pReport->sOrderNum).data());

    do
    {
        updated_order = std::make_shared<Order>(*order.get());
        updated_order->cancelled_ = true;
    } while (!atomic_order->compare_exchange_weak(order, updated_order, std::memory_order_release, std::memory_order_relaxed));

    if (pending_order_request_.load(std::memory_order_relaxed) == client_order_id)
    {
        SPDLOG_DEBUG("CancelReport: Reset pending order request");
        pending_order_request_.store(0, std::memory_order_relaxed);
    }
    *aiCode = API_OK;
    return OK;
}

int RithmicClient::FailureReport(RApi::OrderFailureReport * pReport, void * pContext, int * aiCode)
{
    auto tag = to_string_view(pReport->sTag);
    if (tag.substr(0, 6) != "ZORRO_")
    {
        // order placed by other application, try next order
        *aiCode = API_OK;
        return OK;
    }

    SPDLOG_DEBUG("FailureReport: {} {} order_num={} context={:x} exch_ord_id={} cancelled_size={} status={} reportId={} text={} sUserMsg={}",
        to_string_view(pReport->sTicker), tag, to_string_view(pReport->sOrderNum), reinterpret_cast<uintptr_t>(pReport->pContext), to_string_view(pReport->sExchOrdId),
        pReport->llCancelledSize, to_string_view(pReport->sStatus), to_string_view(pReport->sReportId),
        to_string_view(pReport->sText), to_string_view(pReport->sUserMsg));

    if (pReport->iType == RApi::MD_UPDATE_CB)
    {
        uint64_t client_order_id = atoll(tag.substr(6).data());
        auto *atomic_order = static_cast<std::atomic<std::shared_ptr<Order>>*>(pReport->pContext);
        if (!atomic_order || atomic_order < &orders_.front() || atomic_order > &orders_.back())
        {
            // order placed by other application
            *aiCode = API_OK;
            return OK;
        }

        auto order = atomic_order->load(std::memory_order_relaxed);
        std::shared_ptr<Order> updated_order;

        if (order->client_order_id_ != client_order_id)
        {
            SPDLOG_ERROR("ClientOrderId mismatch: {} {} orderNum: {} conext client_order_id: {}", to_string_view(pReport->sTicker), to_string_view(pReport->sTag), to_string_view(pReport->sOrderNum),
                order->client_order_id_);
            *aiCode = API_OK;
            return OK;
        }

        do
        {
            updated_order = std::make_shared<Order>(*order.get());
            updated_order->text_ = to_string_view(pReport->sText);
        } while (!atomic_order->compare_exchange_weak(order, updated_order, std::memory_order_release, std::memory_order_relaxed));
        
        if (pending_order_request_.load(std::memory_order_relaxed) == client_order_id)
        {
            SPDLOG_DEBUG("FailureReport: Reset pending order request");
            pending_order_request_.store(0, std::memory_order_relaxed);
        }
    }

    *aiCode = API_OK;
    return OK;
}

int RithmicClient::FillReport(RApi::OrderFillReport * pReport, void * pContext, int * aiCode)
{
    auto tag = to_string_view(pReport->sTag);
    if (tag.substr(0, 6) != "ZORRO_")
    {
        // order placed by other application, try next order
        *aiCode = API_OK;
        return OK;
    }

    SPDLOG_INFO("FillReport: {} {} context={:0x} order_num={} exch_ord_id={} fill_price={} fill_qty={} total_fill_qty={}", to_string_view(pReport->sTicker), to_string_view(pReport->sTag), reinterpret_cast<uintptr_t>(pReport->pContext),
        to_string_view(pReport->sOrderNum), to_string_view(pReport->sExchOrdId), pReport->bFillPriceFlag ? pReport->dFillPrice : NAN, pReport->llFillSize, pReport->llTotalFilled);

    if (pReport->iType != RApi::MD_UPDATE_CB)
    {
        *aiCode = API_OK;
        return OK;
    }

    auto *atomic_order = static_cast<std::atomic<std::shared_ptr<Order>>*>(pReport->pContext);
    if (!atomic_order || atomic_order < &orders_.front() || atomic_order > &orders_.back())
    {
        // order placed by other application
        *aiCode = API_OK;
        return OK;
    }

    uint64_t client_order_id = atoll(tag.substr(6).data());
    auto order = atomic_order->load(std::memory_order_relaxed);
    std::shared_ptr<Order> updated_order;

    if (order->client_order_id_ != client_order_id)
    {
        SPDLOG_ERROR("ClientOrderId mismatch: {} {} orderNum: {} conext client_order_id: {}", to_string_view(pReport->sTicker), to_string_view(pReport->sTag), to_string_view(pReport->sOrderNum),
            order->client_order_id_);
        *aiCode = API_OK;
        return OK;
    }

    do
    {
        updated_order = std::make_shared<Order>(*order.get());
        if (pReport->bAvgFillPriceFlag)
        {
            updated_order->avg_fill_price_ = pReport->dPriceToFill;
        }

        if (pReport->bFillPriceFlag)
        {
            updated_order->last_filled_price_ = pReport->bFillPriceFlag;
        }

        updated_order->last_filled_qty_ = pReport->llFillSize;
        updated_order->exec_qty_ = pReport->llTotalFilled;
    } while (!atomic_order->compare_exchange_weak(order, updated_order, std::memory_order_release, std::memory_order_relaxed));

    *aiCode = API_OK;
    return OK;
}

int RithmicClient::ModifyReport(RApi::OrderModifyReport * pReport, void * pContext, int * aiCode)
{
    SPDLOG_DEBUG("ModifyReport: {} {} context={:0x} order_num={} exch_ord_id={}", to_string_view(pReport->sTicker), to_string_view(pReport->sTag),
        reinterpret_cast<uintptr_t>(pReport->pContext), to_string_view(pReport->sOrderNum), to_string_view(pReport->sExchOrdId));
    *aiCode = API_OK;
    return OK;
}

int RithmicClient::NotCancelledReport(RApi::OrderNotCancelledReport * pReport, void * pContext, int * aiCode)
{
    SPDLOG_DEBUG("NotCancelledReport: {} {} context={:0x} order_num={} exch_ord_id={}", to_string_view(pReport->sTicker), to_string_view(pReport->sTag),
        reinterpret_cast<uintptr_t>(pReport->pContext), to_string_view(pReport->sOrderNum), to_string_view(pReport->sExchOrdId));
    *aiCode = API_OK;
    return OK;
}

int RithmicClient::NotModifiedReport(RApi::OrderNotModifiedReport * pReport, void * pContext, int * aiCode)
{
    SPDLOG_DEBUG("NotModifiedReport: {} {} context={:0x} order_num={} exch_ord_id={}", to_string_view(pReport->sTicker), to_string_view(pReport->sTag),
        reinterpret_cast<uintptr_t>(pReport->pContext), to_string_view(pReport->sOrderNum), to_string_view(pReport->sExchOrdId));
    *aiCode = API_OK;
    return OK;
}

int RithmicClient::RejectReport(RApi::OrderRejectReport * pReport, void * pContext, int * aiCode)
{
    auto tag = to_string_view(pReport->sTag);
    if (tag.substr(0, 6) != "ZORRO_")
    {
        // order placed by other application, try next order
        *aiCode = API_OK;
        return OK;
    }

    if (pReport->iType != RApi::MD_UPDATE_CB)
    {
        *aiCode = API_OK;
        return OK;
    }

    SPDLOG_INFO("RejectReport: {} {} context={:0x} order_num={} exch_ord_id={} rejected_size={} replacement_to_follow={}",
        to_string_view(pReport->sTicker), to_string_view(pReport->sTag), reinterpret_cast<uintptr_t>(pReport->pContext),
        to_string_view(pReport->sOrderNum), to_string_view(pReport->sExchOrdId), pReport->llRejectedSize, pReport->bReplacementOrderToFollow);

    auto *atomic_order = static_cast<std::atomic<std::shared_ptr<Order>>*>(pReport->pContext);
    if (!atomic_order || atomic_order < &orders_.front() || atomic_order > &orders_.back())
    {
        // order placed by other application
        *aiCode = API_OK;
        return OK;
    }

    uint64_t client_order_id = atoll(tag.substr(6).data());
    auto order = atomic_order->load(std::memory_order_relaxed);
    std::shared_ptr<Order> updated_order;

    if (order->client_order_id_ != client_order_id)
    {
        SPDLOG_ERROR("ClientOrderId mismatch: {} {} orderNum: {} conext client_order_id: {}", to_string_view(pReport->sTicker), to_string_view(pReport->sTag), to_string_view(pReport->sOrderNum),
            order->client_order_id_);
        *aiCode = API_OK;
        return OK;
    }

    do
    {
        updated_order = std::make_shared<Order>(*order.get());
        updated_order->text_ = "Order Rejected";
    } while (!atomic_order->compare_exchange_weak(order, updated_order, std::memory_order_release, std::memory_order_relaxed));
    

    if (pending_order_request_.load(std::memory_order_relaxed) == client_order_id)
    {
        SPDLOG_DEBUG("RejectReport: Reset pending order request");
        pending_order_request_.store(0, std::memory_order_relaxed);
    }

    *aiCode = API_OK;
    return OK;
}

int RithmicClient::StatusReport(RApi::OrderStatusReport * pReport, void * pContext, int * aiCode)
{
    auto tag = to_string_view(pReport->sTag);
    if (tag.substr(0, 6) != "ZORRO_")
    {
        // order placed by other application, try next order
        *aiCode = API_OK;
        return OK;
    }

    SPDLOG_DEBUG("StatusReport: {} {} context={:0x} order_num={} exch_ord_id={} confirmedSize={}", to_string_view(pReport->sTicker), to_string_view(pReport->sTag),
        reinterpret_cast<uintptr_t>(pReport->pContext), to_string_view(pReport->sOrderNum), to_string_view(pReport->sExchOrdId), pReport->llConfirmedSize);
    
    *aiCode = API_OK;
    return OK;
}

int RithmicClient::TriggerReport(RApi::OrderTriggerReport * pReport, void * pContext, int * aiCode)
{
    SPDLOG_DEBUG("TriggerReport: {} {} context={:0x} order_num={} exch_ord_id={}", to_string_view(pReport->sTicker), to_string_view(pReport->sTag),
        reinterpret_cast<uintptr_t>(pReport->pContext), to_string_view(pReport->sOrderNum), to_string_view(pReport->sExchOrdId));
    *aiCode = API_OK;
    return OK;
}

int RithmicClient::OtherReport(RApi::OrderReport * pReport, void * pContext, int * aiCode)
{
    SPDLOG_DEBUG("OtherReport: {} {} context={:0x} order_num={} exch_ord_id={}", to_string_view(pReport->sTicker), to_string_view(pReport->sTag),
        reinterpret_cast<uintptr_t>(pReport->pContext), to_string_view(pReport->sOrderNum), to_string_view(pReport->sExchOrdId));
    *aiCode = API_OK;
    return OK;
}

bool RithmicClient::cancelOrder(uint32_t order_id)
{
    auto iter = orders_by_id_.find(order_id);
    if (iter == orders_by_id_.end())
    {
        BrokerError(std::format("Order {} not found", order_id).c_str());
        return false;
    }

    auto &atomic_order = *iter->second;
    auto order = atomic_order.load(std::memory_order_relaxed);
    if (order->cancelled_)
    {
        return true;
    }

    SPDLOG_INFO("Cancel order: {}", order_id);
    tsNCharcb order_num { order->str_order_num_.data(), (int)order->str_order_num_.length() };
    int iCode;
    pending_order_request_.store(order->client_order_id_, std::memory_order_relaxed);
    if (!engine_->cancelOrder(&account_info_, &order_num, (tsNCharcb*)&sORDER_ENTRY_TYPE_AUTO, nullptr, nullptr, &atomic_order, &iCode))
    {
        BrokerError(std::format("Failed to cancel {}. err: {}", order_id, iCode).c_str());
        return false;
    }

    while (pending_order_request_.load(std::memory_order_relaxed))
    {
        if (!BrokerProgress(1))
        {
            return false;
        }
    }

    auto updated_order = atomic_order.load(std::memory_order_relaxed);
    if (!updated_order->text_.empty())
    {
        BrokerError(updated_order->text_.c_str());
    }

    return true;
}