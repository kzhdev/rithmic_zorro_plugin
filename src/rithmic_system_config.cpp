#include "rithmic_system_config.h"
#include <toml.hpp>

using namespace zorro;

RithmicSystemConfig::RithmicSystemConfig(const std::string& config_file)
{
    try
    {
        auto config = toml::parse_file(config_file);
        for (const auto& [key, value] : config)
        {
            if (value.is_table())
            {
                RithimcServerInfo server_info;
                auto table = value.as_table();
                if (table->contains("MML_DMN_SRVR_ADDR"))
                {
                    if (auto value = table->at("MML_DMN_SRVR_ADDR").as<std::string>()) [[likely]]
                    {
                        server_info.MML_DMN_SRVR_ADDR = std::format(RithimcServerInfo::MML_DMN_SRVR_ADDR_template, value->get());
                    }
                    else
                    {
                        throw std::runtime_error(std::format("MML_DMN_SRVR_ADDR in {} is not a string.", std::string(key)));
                    }
                }
                else
                {
                    throw std::runtime_error(std::format("MML_DMN_SRVR_ADDR not found in {}.", std::string(key)));
                }
                if (table->contains("MML_DOMAIN_NAME"))
                {
                    if (auto value = table->at("MML_DOMAIN_NAME").as<std::string>()) [[likely]]
                    {
                        server_info.MML_DOMAIN_NAME = std::format(RithimcServerInfo::MML_DOMAIN_NAME_template, value->get());
                    }
                    else
                    {
                        throw std::runtime_error(std::format("MML_DOMAIN_NAME in {} is not a string.", std::string(key)));
                    }
                }
                else
                {
                    throw std::runtime_error(std::format("MML_DOMAIN_NAME not found in {}.", std::string(key)));
                }
                if (table->contains("MML_LIC_SRVR_ADDR"))
                {
                    if (auto value = table->at("MML_LIC_SRVR_ADDR").as<std::string>()) [[likely]]
                    {
                        server_info.MML_LIC_SRVR_ADDR = std::format(RithimcServerInfo::MML_LIC_SRVR_ADDR_template, value->get());
                    }
                    else
                    {
                        throw std::runtime_error(std::format("MML_LIC_SRVR_ADDR in {} is not a string.", std::string(key)));
                    }
                }
                else
                {
                    throw std::runtime_error(std::format("MML_LIC_SRVR_ADDR not found in {}.", std::string(key)));
                }
                if (table->contains("MML_LOC_BROK_ADDR"))
                {
                    if (auto value = table->at("MML_LOC_BROK_ADDR").as<std::string>())
                    {
                        server_info.MML_LOC_BROK_ADDR = std::format(RithimcServerInfo::MML_LOC_BROK_ADDR_template, value->get());
                    }
                    else
                    {
                        throw std::runtime_error(std::format("MML_LOC_BROK_ADDR in {} is not a string.", std::string(key)));
                    }
                }
                else
                {
                    throw std::runtime_error(std::format("MML_LOC_BROK_ADDR not found in {}.", std::string(key)));
                }
                if (table->contains("MML_LOGGER_ADDR"))
                {
                    if (auto value = table->at("MML_LOGGER_ADDR").as<std::string>())
                    {
                        server_info.MML_LOGGER_ADDR = std::format(RithimcServerInfo::MML_LOGGER_ADDR_template, value->get());
                    }
                    else
                    {
                        throw std::runtime_error(std::format("MML_LOGGER_ADDR in {} is not a string.", std::string(key)));
                    }
                }
                else
                {
                    throw std::runtime_error(std::format("MML_LOGGER_ADDR not found in {}.", std::string(key)));
                }
                if (table->contains("MML_LOG_TYPE"))
                {
                    if (auto value = table->at("MML_LOG_TYPE").as<std::string>())
                    {
                        server_info.MML_LOG_TYPE = std::format(RithimcServerInfo::MML_LOG_TYPE_template, value->get());
                    }
                    else
                    {
                        throw std::runtime_error(std::format("MML_LOG_TYPE in {} is not a string.", std::string(key)));
                    }
                }
                else
                {
                    server_info.MML_LOG_TYPE = "MML_LOG_TYPE=log_net";
                }
                if (table->contains("MML_SSL_CLNT_AUTH_FILE"))
                {
                    if (auto value = table->at("MML_SSL_CLNT_AUTH_FILE").as<std::string>())
                    {
                        server_info.MML_SSL_CLNT_AUTH_FILE = std::format(RithimcServerInfo::MML_SSL_CLNT_AUTH_FILE_template, value->get());
                    }
                    else
                    {
                        throw std::runtime_error(std::format("MML_SSL_CLNT_AUTH_FILE in {} is not a string.", std::string(key)));
                    }
                }
                else
                {
                    server_info.MML_SSL_CLNT_AUTH_FILE = "MML_SSL_CLNT_AUTH_FILE=rithmic_ssl_cert_auth_params";
                }
                server_names.push_back(std::string(key));
                servers.emplace(std::string(key), server_info);
            }
        }
    }
    catch (const toml::parse_error& err)
    {
        SPDLOG_ERROR("Error parsing config file: {}", err.description());
        throw std::runtime_error(std::format("Failed to parse rithmic_config.toml file. {}", err.description()));
    }
}

void RithmicSystemConfig::setServer(const std::string& server_name)
{
    auto it = servers.find(server_name);
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
        throw std::runtime_error(std::format("Server {} not found in rithimc_config.toml.", server_name));
    }
}