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
#include <string>
#include <cstdint>
#include <atomic>
#include <array>
#include <bitset>

namespace zorro {

enum class Side : uint8_t;

struct Spec
{
    std::string symbol_;
    std::string exchange_;
    std::string product_;
    std::string ticker_;
    std::string description_;
    std::string type_;
    std::string currency_;

    double cap_price_ = NAN;
    double floor_price_ = NAN;
    double strike_price_ = NAN;
    double size_muliplier = 1.0;
    double price_increment_ = 0.0;
    int32_t size_multiplier_precision_ = 0;
    int64_t min_size_increment_ = 0;
    bool tradable_ = false;
};

struct MDTop
{
    double bid_price_ = NAN;
    double ask_price_ = NAN;
    int64_t bid_qty_ = 0;
    int64_t ask_qty_ = 0;
};

struct Trade
{
    Side side_;
    double price_ = NAN;
    int64_t qty_ = 0;
    uint64_t time_ = 0;
    uint64_t buy_volume_ = 0;   // total daily buy volume
    uint64_t sell_volume_ = 0;   // total daily sell volume
};

struct MDStatus
{
    uint64_t time_ = 0;
    bool is_open_ = false;
};

enum MDReady : uint8_t
{
    Top,
    Status,
    __count__,  // number of MDReady, internal use only
};

inline const char* to_string(MDReady ready)
{
    static constexpr std::array<const char*, 2> ready_str = {"Top", "Status"};
    static_assert(ready_str.size() == MDReady::__count__, "Invalid MDReady");
    return ready_str[(uint8_t)ready];
}

struct Symbol
{
    Spec spec_;
    std::atomic_bool can_trade_;
    std::atomic<MDTop> top_;
    std::atomic<Trade> last_trade_;
    std::atomic<std::bitset<3>> ready_;

    Symbol() = default;

    Symbol(const Symbol &other)
        : spec_(other.spec_)
        , can_trade_{other.can_trade_.load(std::memory_order_relaxed)}
        , top_{other.top_.load(std::memory_order_relaxed)}
        , ready_{other.ready_.load(std::memory_order_relaxed)}
    {}

    Symbol& operator=(const Symbol &other)
    {
        spec_ = other.spec_;
        can_trade_.store(other.can_trade_.load(std::memory_order_relaxed));
        top_.store(other.top_.load(std::memory_order_relaxed));
        ready_.store(other.ready_.load(std::memory_order_relaxed));
        return *this;
    }
};

}   // end namesapce zorro