// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include <iostream>
#include <sstream>
#include <cmath>
#include <iterator>
#include <rdma/rdma_cma.h>
#include <boost/lexical_cast.hpp>

/// Overloaded output operator for STL vectors.
template <class T>
inline std::ostream& operator<<(std::ostream& os, const std::vector<T>& v)
{
    copy(v.begin(), v.end(), std::ostream_iterator<T>(os, " "));
    return os;
}

/// Overloaded output operator for ibv_wr_opcode.
inline std::ostream& operator<<(std::ostream& s, ibv_wr_opcode v)
{
    switch (v) {
    case IBV_WR_RDMA_WRITE:
        return s << "IBV_WR_RDMA_WRITE";
    case IBV_WR_RDMA_WRITE_WITH_IMM:
        return s << "IBV_WR_RDMA_WRITE_WITH_IMM";
    case IBV_WR_SEND:
        return s << "IBV_WR_SEND";
    case IBV_WR_SEND_WITH_IMM:
        return s << "IBV_WR_SEND_WITH_IMM";
    case IBV_WR_RDMA_READ:
        return s << "IBV_WR_RDMA_READ";
    case IBV_WR_ATOMIC_CMP_AND_SWP:
        return s << "IBV_WR_ATOMIC_CMP_AND_SWP";
    case IBV_WR_ATOMIC_FETCH_AND_ADD:
        return s << "IBV_WR_ATOMIC_FETCH_AND_ADD";
    default:
        return s << static_cast<int>(v);
    }
}

/// Overloaded output operator for ibv_send_flags.
inline std::ostream& operator<<(std::ostream& s, ibv_send_flags v)
{
    std::string str;
    if (v & IBV_SEND_FENCE)
        str += std::string(str.empty() ? "" : " | ") + "IBV_SEND_FENCE";
    if (v & IBV_SEND_SIGNALED)
        str += std::string(str.empty() ? "" : " | ") + "IBV_SEND_SIGNALED";
    if (v & IBV_SEND_SOLICITED)
        str += std::string(str.empty() ? "" : " | ") + "IBV_SEND_SOLICITED";
    if (v & IBV_SEND_INLINE)
        str += std::string(str.empty() ? "" : " | ") + "IBV_SEND_INLINE";
    if (str.empty())
        return s << static_cast<int>(v);
    else
        return s << str;
}

inline std::string human_readable_count(uint64_t bytes, bool use_si = false,
                                        std::string unit_string = "B")
{
    uint64_t unit = use_si ? 1000 : 1024;

    if (bytes < unit)
        return boost::lexical_cast<std::string>(bytes) + " " + unit_string;

    uint32_t exponent = static_cast<uint64_t>(std::log(bytes) / std::log(unit));

    std::string prefix =
        std::string(use_si ? "kMGTPE" : "KMGTPE").substr(exponent - 1, 1);
    if (!use_si)
        prefix += "i";

    std::stringstream st;
    st.precision(4);
    st << (bytes / std::pow(unit, exponent)) << " " << prefix << unit_string;

    return st.str();
}

template <typename T>
inline std::string bar_graph(std::vector<T> values, std::string symbols,
                             uint32_t length)
{
    std::string s;
    s.reserve(length);

    T sum = 0;
    for (T n : values) {
        sum += n;
    }

    float filled = 0.0;
    for (size_t i = 0; i < values.size(); ++i) {
        filled += static_cast<float>(values[i]) / static_cast<float>(sum);
        uint32_t chars =
            static_cast<uint32_t>(round(filled * static_cast<float>(length)));
        s.append(std::string(chars - s.size(), symbols[i % symbols.size()]));
    }

    return s;
}
