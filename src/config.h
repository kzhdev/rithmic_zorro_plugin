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

#include <cstdint>
#include <string>
#include <bitset>
#include <fstream>
#include <spdlog/spdlog.h>
#include <format>

namespace {
    inline void ltrim(std::string& s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); }));
    }

    inline void rtrim(std::string& s) {
        s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end());
    }

    inline void trim(std::string& s) {
        ltrim(s);
        rtrim(s);
    }
}

namespace zorro {

    struct Config {
        uint8_t log_level_ = spdlog::level::info;
        std::string rithmic_config_path_ = "rithmic_config.toml";

        static Config& get()
        {
            static Config instance;
            return instance;
        }

        Config(const Config&) = delete;
        Config(Config&&) = delete;
        Config& operator=(const Config&) = delete;
        Config& operator=(Config&&) = delete;

    private:
        Config()
        {
            readConfig("./Zorro.ini");
            readConfig("./ZorroFix.ini");
        }
        ~Config() = default;

        inline bool readConfig(const char* file)
        {
            std::ifstream config(file);
            if (!config.is_open())
            {
                return false;
            }

            std::string line;

            while (getline(config, line))
            {
                ltrim(line);
                if (line.find("//") == 0) {
                    continue;
                }

                getConfig(line, ConfigFound::cf_LogLevel, "RithmicLogLevel", log_level_);
                getConfig(line, ConfigFound::cf_RithmicConfigPath, "RithmicConfigPath", rithmic_config_path_);
            }
            config.close();
            return configFound_.all();
        }

        template<typename T>
        inline void getConfig(std::string& line, uint8_t found_flag, const std::string& config_name, T& value, bool combine_name_value = false) {
            auto pos = line.find(config_name);
            if (pos == std::string::npos) {
                return;
            }
            auto pos1 = line.find("=");
            if (pos1 == std::string::npos) {
                return;
            }

            std::string v = line.substr(pos1 + 1);
            pos1 = v.rfind("//");
            if (pos1 != std::string::npos) {
                v = v.substr(0, pos1);
            }

            trim(v);
            setValue(config_name, value, v, combine_name_value);
            configFound_.set(found_flag, 1);
        }

        template<typename T>
        inline typename std::enable_if<std::is_same<T, std::string>::value>::type setValue(const std::string& config_name, T& value, const std::string& v, bool combine_name_value) { 
            value = v;
            // remove ""
            value.erase(std::remove(value.begin(), value.end(), '"'), value.end());
            if (combine_name_value)
            {
                value = std::format("{}={}", config_name, value);
            }
        }

        template<typename T>
        inline typename std::enable_if<std::is_integral<T>::value>::type setValue(const std::string& config_name, T& value, const std::string& v, bool combine_name_value) { value = atoi(v.c_str()); }

        template<typename T>
        inline typename std::enable_if<std::is_floating_point<T>::value>::type setValue(const std::string& config_name, T& value, const std::string& v, bool combine_name_value) { value = atof(v.c_str()); }


    private:
        enum ConfigFound : uint8_t
        {
            cf_LogLevel,
            cf_RithmicConfigPath,
            __count__,  // for internal use only
        };
        std::bitset<ConfigFound::__count__> configFound_ = 0;
    };
}