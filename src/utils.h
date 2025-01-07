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

#include <string>
#include <string_view>
#include <RApiPlus.h>
#include <date/date.h>

typedef double DATE;

inline bool operator==(const tsNCharcb &a, const tsNCharcb &b)
{
    return a.iDataLen == b.iDataLen 
        && (a.pData == b.pData || strncmp(a.pData, b.pData, a.iDataLen) == 0);
}

namespace zorro {

inline std::string to_string(const tsNCharcb &nchar)
{
    return std::string(nchar.pData, nchar.iDataLen);
}

inline std::string_view to_string_view(const tsNCharcb &nchar)
{
    return std::string_view(nchar.pData, nchar.iDataLen);
}

template<typename infoT>
inline std::string symbol(const infoT *info)
{
    return std::format("{}.{}", to_string_view(info->sTicker), to_string_view(info->sExchange));
}

inline DATE convertTime(__time32_t t32)
{
    return (DATE)t32 / 86400. + 25569.; // 25569. = DATE(1.1.1970 00:00)
}

inline __time32_t convertTime(DATE date)
{
    return (__time32_t)((date - 25569.) * 86400.);
}

inline std::string timeToString(__time32_t time) {
    return date::format("%FT%TZ", date::sys_seconds{ std::chrono::seconds{ time } });
}

inline uint64_t get_timestamp() 
{
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

template<typename T>
inline uint64_t nanosec(const T &info)
{
    return (uint64_t)info.iSsboe * 1000000000 + info.iUsecs * 1000;
}

template<>
inline uint64_t nanosec<RApi::TradeInfo>(const RApi::TradeInfo &info)
{
    return (uint64_t)info.iSourceSsboe * 1000000000 + info.iSourceUsecs * 1000 + info.iSourceNsecs;
}

inline Side to_side(const RApi::TradeInfo &info)
{
    if (info.sAggressorSide.pData)
    {
        return (info.sAggressorSide.pData[0] == 'B' ? Side::Buy : Side::Sell);
    }
    return Side::Unknown;
}

} // namespace zorro