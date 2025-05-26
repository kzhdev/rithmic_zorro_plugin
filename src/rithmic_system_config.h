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

#include <unordered_map>
#include <string>
#include <array>

namespace zorro
{
    struct RithmicSystemConfig
    {
        struct RithimcServerInfo
        {
            static constexpr const char* MML_DMN_SRVR_ADDR_template = "MML_DMN_SRVR_ADDR={}";
            static constexpr const char* MML_DOMAIN_NAME_template = "MML_DOMAIN_NAME={}";
            static constexpr const char* MML_LIC_SRVR_ADDR_template = "MML_LIC_SRVR_ADDR={}";
            static constexpr const char* MML_LOC_BROK_ADDR_template = "MML_LOC_BROK_ADDR={}";
            static constexpr const char* MML_LOGGER_ADDR_template = "MML_LOGGER_ADDR={}";
            static constexpr const char* MML_LOG_TYPE_template = "MML_LOG_TYPE={}";
            static constexpr const char* MML_SSL_CLNT_AUTH_FILE_template = "MML_SSL_CLNT_AUTH_FILE={}";

            // Default to Rmithmic Test Server
            std::string MML_DMN_SRVR_ADDR = "MML_DMN_SRVR_ADDR=rituz00100.00.rithmic.com:65000~rituz00100.00.rithmic.net:65000~rituz00100.00.theomne.net:65000~rituz00100.00.theomne.com:65000";
            std::string MML_DOMAIN_NAME = "MML_DOMAIN_NAME=rithmic_uat_dmz_domain";
            std::string MML_LIC_SRVR_ADDR = "MML_LIC_SRVR_ADDR=rituz00100.00.rithmic.com:56000~rituz00100.00.rithmic.net:56000~rituz00100.00.theomne.net:56000~rituz00100.00.theomne.com:56000";
            std::string MML_LOC_BROK_ADDR = "MML_LOC_BROK_ADDR=rituz00100.00.rithmic.com:64100";
            std::string MML_LOGGER_ADDR = "MML_LOGGER_ADDR=rituz00100.00.rithmic.com:45454~rituz00100.00.rithmic.net:45454~rituz00100.00.theomne.com:45454~rituz00100.00.theomne.net:45454";
            std::string MML_LOG_TYPE = "MML_LOG_TYPE=log_net";
            std::string MML_SSL_CLNT_AUTH_FILE = "MML_SSL_CLNT_AUTH_FILE=rithmic_ssl_cert_auth_params"; 
        };

        std::vector<std::string> server_names;
        std::unordered_map<std::string, RithimcServerInfo> servers;

        std::array<char*, 9> env = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};

        RithmicSystemConfig(const std::string& config_file);

        void setServer(const std::string& server_name);
    };
}