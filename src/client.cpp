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
#include <version.h>
#include "config.h"
#include "utils.h"

using namespace zorro;
using namespace RApi;

RithmicClient::RithmicClient(std::string user)
    : system_config_(Config::get().rithmic_config_path_)
    , pid_(GetCurrentProcessId())
    , user_(user)
    , env_user_("USER=" + std::move(user))
{
    system_config_.env[7] = (char*)env_user_.data();
    ticks_.reserve(15000);
}

RithmicClient::~RithmicClient()
{
    if (engine_)
    {
        int iIgnored;
        engine_->logout(&iIgnored);
    }
}

void RithmicClient::setServer(const std::string &server_name)
{
    BrokerError(std::format("Connecting to server: {}", server_name).c_str());
    system_config_.setServer(server_name);
    
    engine_params_.sAppName.pData = (char*)"RithmicZorroPlugin";
    engine_params_.sAppName.iDataLen = (int)strlen(engine_params_.sAppName.pData);
    engine_params_.sAppVersion.pData = (char*)VERSION;
    engine_params_.sAppVersion.iDataLen = (int)strlen(engine_params_.sAppVersion.pData);
    engine_params_.envp = system_config_.env.data();
    engine_params_.pAdmCallbacks = &adm_callbacks_;
    engine_params_.sLogFilePath.pData = (char*)"./Log/rithmic.log";
    engine_params_.sLogFilePath.iDataLen = (int)strlen(engine_params_.sLogFilePath.pData);

    engine_ = std::make_unique<REngine>(&engine_params_);
}


std::string_view RithmicClient::accountId() const noexcept
{
    return to_string_view(account_info_.sAccountId);
}

bool RithmicClient::login(std::string password, std::string &err)
{
    password_ = std::move(password);

    // if (!checkAgreements(err))
    // {
    //     return false;
    // }

    LoginParams params;
    params.pCallbacks = this;
    params.sMdUser.pData = (char*)user_.data();
    params.sMdUser.iDataLen = (int)strlen(params.sMdUser.pData);
    params.sMdPassword.pData = (char*)password_.data();
    params.sMdPassword.iDataLen = (int)strlen(params.sMdPassword.pData);
    params.sMdCnnctPt.pData = (char*)"login_agent_tpc";
    params.sMdCnnctPt.iDataLen = (int)strlen(params.sMdCnnctPt.pData);

    params.sTsUser = params.sMdUser;
    params.sTsPassword = params.sMdPassword;
    params.sTsCnnctPt.pData = (char*)"login_agent_opc";
    params.sTsCnnctPt.iDataLen = (int)strlen(params.sTsCnnctPt.pData);

    params.sPnlCnnctPt.pData = (char*)"login_agent_pnlc";
    params.sPnlCnnctPt.iDataLen = (int)strlen(params.sPnlCnnctPt.pData);

    params.sIhUser = params.sMdUser;
    params.sIhPassword = params.sMdPassword;
    params.sIhCnnctPt.pData = (char*)"login_agent_historyc";
    params.sIhCnnctPt.iDataLen = (int)strlen(params.sIhCnnctPt.pData);

    int iCode;
    if (!engine_->login(&params, &iCode))
    {
        err = std::format("REngine::login err: {}", iCode);
        return false;
    }

    unsigned long login_status = 0;
    while((login_status = login_status_.load(std::memory_order_relaxed).to_ulong()) < 15)
    {
        if (!BrokerProgress(1))
        {
            return false;
        }
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(1s);
    }

    if (login_status != 15)
    {
        err = "Login failed";
        return false;
    }
    
    while(!account_received_.load(std::memory_order_relaxed))
    {
        if (!BrokerProgress(1))
        {
            return false;
        }
    }

    if (!listTradeRoutes())
    {
        return false;
    }

    if (!subscribeOrder())
    {
        return false;
    }

    auto last_order_index = next_order_index_.load(std::memory_order_relaxed);
    for (auto i = 0u; i < last_order_index; ++i)
    {
        auto order = orders_[i].load(std::memory_order_relaxed);
        if (order->order_num_ != 0)
        {
            orders_by_id_[order->order_num_] = &orders_[i];
        }
    }

    if (!subscribePnl())
    {
        return false;
    }

    return true;
}

bool RithmicClient::checkAgreements(std::string &err)
{
    tsNCharcb sRepEnvKey;
    tsNCharcb sRepUser;
    tsNCharcb sRepPassword;
    tsNCharcb sRepCnnctPt;

    sRepEnvKey.pData      = (char*)"system";
    sRepEnvKey.iDataLen   = (int)strlen(sRepEnvKey.pData);
    sRepUser.pData        = (char*)user_.data();
    sRepUser.iDataLen     = (int)strlen(sRepUser.pData);
    sRepPassword.pData    = (char*)password_.data();
    sRepPassword.iDataLen = (int)strlen(sRepPassword.pData); 
    sRepCnnctPt.pData     = (char*)"login_agent_repositoryc";
    sRepCnnctPt.iDataLen  = (int)strlen(sRepCnnctPt.pData);

    int iCode;
    request_status_.store(RequestStatus::AwaitingResults, std::memory_order_release);
    if (!engine_->loginRepository(&sRepEnvKey, &sRepUser, &sRepPassword, &sRepCnnctPt, this, &iCode))
    {
        err = std::format("REngine::loginRepository err: {}", iCode);
        return false;
    }

    using namespace std::chrono_literals;
    RequestStatus status;
    while((status = request_status_.load(std::memory_order_relaxed)) == RequestStatus::AwaitingResults)
    {
        std::this_thread::sleep_for(1s);
    }

    if (status != RequestStatus::Complete)
    {
        return false;
    }

    int iIgnored;
    if (!engine_->listAgreements(false, NULL, &iCode))
    {
        err = std::format("listAgreements err: {}", iCode);
        engine_->logoutRepository(&iIgnored);
        return false;
    }

    while (!unaccepted_agreements_received_.load(std::memory_order_relaxed))
    {
        std::this_thread::sleep_for(1s);
    }

    engine_->logoutRepository(&iIgnored);
    return has_unaccepted_aggreements_;
}

int RithmicClient::AgreementList(RApi::AgreementListInfo *pInfo, void * pContext, int *aiCode)
{
    if (!pInfo->bAccepted)
    {
        tsNCharcb sActive = {(char*)"active", 6};
        for (auto i = 0; i < pInfo->iArrayLen; ++i)
        {
            AgreementInfo oAg = pInfo->asAgreementInfoArray[i];
            if (oAg.sStatus.iDataLen == sActive.iDataLen &&
                memcmp(oAg.sStatus.pData, sActive.pData, oAg.sStatus.iDataLen) == 0)
            {
                if (oAg.bMandatory)
                {
                    has_unaccepted_aggreements_ = true;
                    break;
                }
            }
        }
    }
    unaccepted_agreements_received_.store(true, std::memory_order_release);

    *aiCode = API_OK;
    return (OK);
}

int RithmicClient::AccountList(RApi::AccountListInfo *pInfo, void *pContext, int *aiCode)
{
    if (pInfo->iArrayLen)
    {
        auto &account = pInfo->asAccountInfoArray[0];
        fcm_id_ = to_string_view(account.sFcmId);
        ib_id_ = to_string_view(account.sIbId);
        account_id_ = to_string_view(account.sAccountId);
        account_name_ = to_string_view(account.sAccountName);
        account_info_.sFcmId.pData = fcm_id_.data();
        account_info_.sFcmId.iDataLen = (int)fcm_id_.length();
        account_info_.sIbId.pData = ib_id_.data();
        account_info_.sIbId.iDataLen = (int)ib_id_.length();
        account_info_.sAccountId.pData = account_id_.data();
        account_info_.sAccountId.iDataLen = (int)account_id_.length();
        account_info_.sAccountName.pData = account_name_.data();
        account_info_.sAccountName.iDataLen = (int)account_name_.length(); 
        account_received_.store(true, std::memory_order_release);
    }
    *aiCode = API_OK;
    return (OK);
}

bool RithmicClient::listTradeRoutes()
{
    request_status_.store(RequestStatus::AwaitingResults, std::memory_order_relaxed);
    int iCode;
    if (!engine_->listTradeRoutes(nullptr, &iCode))
    {
        BrokerError(std::format("REngine::listTradeRoutes() err: {}", iCode).c_str());
        return false;
    }

    auto status = waitForRequest();
    return status == RequestStatus::Complete;
}

int RithmicClient::TradeRouteList(RApi::TradeRouteListInfo *pInfo, void *pContext, int *aiCode)
{
    if (pInfo->iArrayLen)
    {
        for (auto i = 0; i < pInfo->iArrayLen; ++i)
        {
            auto &trade_route = pInfo->asTradeRouteInfoArray[i];
            SPDLOG_INFO("TradeRoute: {} {} {} {} {}", to_string_view(trade_route.sFcmId), to_string_view(trade_route.sIbId),
                to_string_view(trade_route.sExchange), to_string_view(trade_route.sTradeRoute), to_string_view(trade_route.sStatus));

            if (trade_route.sFcmId.iDataLen == account_info_.sFcmId.iDataLen &&
                memcmp(trade_route.sFcmId.pData, account_info_.sFcmId.pData, trade_route.sFcmId.iDataLen) == 0 &&
                trade_route.sIbId.iDataLen == account_info_.sIbId.iDataLen &&
                memcmp(trade_route.sIbId.pData, account_info_.sIbId.pData, trade_route.sIbId.iDataLen) == 0 &&
                trade_route.sStatus.iDataLen == sTRADE_ROUTE_STATUS_UP.iDataLen &&
                memcmp(trade_route.sStatus.pData, sTRADE_ROUTE_STATUS_UP.pData, trade_route.sStatus.iDataLen) == 0)
            {
                auto exchange = to_string(trade_route.sExchange);
                if (trade_routes_.find(exchange) == trade_routes_.end())
                {
                    trade_routes_.emplace(exchange, to_string_view(trade_route.sTradeRoute));
                }
            }
        }
    }
    request_status_.store(RequestStatus::Complete, std::memory_order_release);
    *aiCode = API_OK;
    return (OK);
}

