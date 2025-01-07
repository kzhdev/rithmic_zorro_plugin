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

namespace zorro {

enum class Side : uint8_t
{
    Buy,
    Sell,
    Unknown,
};

enum class Duration : uint8_t
{
    Day,
    FOK,
    IOC,
    GTC,
    __count__,  // number of Duration, internal use only
};

inline const char* to_string(Duration duration)
{
    static constexpr const char* s_duration[] = {
        "Day",
        "FOK",
        "IOC",
        "GTC",
    };
    static_assert(sizeof(s_duration) / sizeof(s_duration[0]) == (size_t)Duration::__count__, "Duration string array size mismatch");
    return s_duration[static_cast<uint8_t>(duration)];
}

struct Order
{
    std::string symbol_;
    std::string exchange_;
    std::string ticker_;
    std::string exch_ord_id_;
    std::string ticker_plant_exch_ord_id_;

    std::string str_order_num_;
    std::string original_order_num_;
    std::string initial_sequence_number_;
    std::string current_sequence_number_;
    std::string omni_bus_account_;
    std::string tag_;
    std::string text_;
    std::string user_msg_;

    tsNCharcb duration_; 
    tsNCharcb order_type_;
    tsNCharcb original_order_type_;
    tsNCharcb trade_route_;
    tsNCharcb status_;
    tsNCharcb completion_reason_;

    double price_ = NAN;
    double trigger_price_ = NAN;
    double avg_fill_price_ = NAN;
    double last_filled_price_ = NAN;

    uint64_t qty_ = 0;
    uint64_t exec_qty_ = 0;
    uint64_t last_filled_qty_ = 0;
    uint64_t last_update_time_ = 0;
    uint64_t client_order_id_ = 0;
    int32_t index_ = -1;
    uint32_t order_num_ = 0;
    Side side_ = Side::Buy;

    bool pending_cancel_ = false;
    bool cancelled_ = false;
};

}