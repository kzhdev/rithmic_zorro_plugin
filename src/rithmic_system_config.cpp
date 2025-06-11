#include "rithmic_system_config.h"
#include <nlohmann/json.hpp>
#include <base64_encode_decode.h>

using namespace zorro;
using json = nlohmann::json;

namespace {

// Helper: replace the first occurrence of "{}" in tmpl with value
std::string applyTemplate(const char* tmpl, const std::string& value)
{
    std::string s(tmpl);
    auto pos = s.find("{}");
    if (pos != std::string::npos)
    {
        s.replace(pos, 2, value);
    }
    return s;
}

}   // namespace

RithmicSystemConfig::RithmicSystemConfig(const std::string &rimthic_config_path)
{
    try
    {
        std::ifstream file(rimthic_config_path, std::ios::binary);
        if (!file.is_open())
        {
            throw std::runtime_error(std::format("Failed to open rithmic config {}", rimthic_config_path));
        }

        file.seekg(0, std::ios::end);
        size_t fileSize = file.tellg();
        file.seekg(0, std::ios::beg);

        std::string fileContent;
        fileContent.resize(fileSize);
        file.read(&fileContent[0], fileSize);

        file.close();

        auto json_str = base64_decode(fileContent);
        json j = json::parse(json_str, nullptr, false);
        if (j.is_discarded())
        {
            throw std::runtime_error("Failed to parse rithmic config JSON.");
        }

        // Iterate systems
        for (auto& [system, gateways] : j.items())
        {
            for (auto& [gateway, params] : gateways.items())
            {
                RithimcServerInfo info;

                if (params.contains("MML_DMN_SRVR_ADDR"))
                    info.MML_DMN_SRVR_ADDR = applyTemplate(
                        RithimcServerInfo::MML_DMN_SRVR_ADDR_template,
                        params["MML_DMN_SRVR_ADDR"].get<std::string>());

                if (params.contains("MML_DOMAIN_NAME"))
                    info.MML_DOMAIN_NAME = applyTemplate(
                        RithimcServerInfo::MML_DOMAIN_NAME_template,
                        params["MML_DOMAIN_NAME"].get<std::string>());

                if (params.contains("MML_LIC_SRVR_ADDR"))
                    info.MML_LIC_SRVR_ADDR = applyTemplate(
                        RithimcServerInfo::MML_LIC_SRVR_ADDR_template,
                        params["MML_LIC_SRVR_ADDR"].get<std::string>());

                if (params.contains("MML_LOC_BROK_ADDR"))
                    info.MML_LOC_BROK_ADDR = applyTemplate(
                        RithimcServerInfo::MML_LOC_BROK_ADDR_template,
                        params["MML_LOC_BROK_ADDR"].get<std::string>());

                if (params.contains("MML_LOGGER_ADDR"))
                    info.MML_LOGGER_ADDR = applyTemplate(
                        RithimcServerInfo::MML_LOGGER_ADDR_template,
                        params["MML_LOGGER_ADDR"].get<std::string>());

                if (params.contains("MML_LOG_TYPE"))
                    info.MML_LOG_TYPE = applyTemplate(
                        RithimcServerInfo::MML_LOG_TYPE_template,
                        params["MML_LOG_TYPE"].get<std::string>());

                // SSL auth file always present
                if (params.contains("MML_SSL_CLNT_AUTH_FILE"))
                    info.MML_SSL_CLNT_AUTH_FILE = applyTemplate(
                        RithimcServerInfo::MML_SSL_CLNT_AUTH_FILE_template,
                        params["MML_SSL_CLNT_AUTH_FILE"].get<std::string>());

                // Build the map key: "<system>_<gateway>"
                std::string key = std::format("{}_{}", system, gateway);
                servers.emplace(std::move(key), std::move(info));
                server_gateways[system].emplace_back(std::move(gateway));
            }
        }
    }
    catch (const std::exception& err)
    {
        SPDLOG_ERROR("Error parsing config: {}", err.what());
        throw std::runtime_error(std::format("Failed to parse rithmic config. {}", err.what()));
    }
}

void RithmicSystemConfig::setServer(const std::string& server_gateway)
{
    auto it = servers.find(server_gateway);
    if (it != servers.end())
    {
        env[0] = it->second.MML_DMN_SRVR_ADDR.data();
        env[1] = it->second.MML_DOMAIN_NAME.data();
        env[2] = it->second.MML_LIC_SRVR_ADDR.data();
        env[3] = it->second.MML_LOC_BROK_ADDR.data();
        env[4] = it->second.MML_LOGGER_ADDR.data();
        env[5] = it->second.MML_LOG_TYPE.data();
        env[6] = it->second.MML_SSL_CLNT_AUTH_FILE.data();
    }
    else
    {
        throw std::runtime_error(std::format("Server {} not found in rithimc_config.toml.", server_gateway));
    }
}