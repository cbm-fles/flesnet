// Copyright 2012-2015 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include <boost/lexical_cast.hpp>
#include <cmath>
#include <iostream>
#include <iterator>
#include <sstream>
#include <vector>

/// Overloaded output operator for STL vectors.
template <class T>
inline std::ostream& operator<<(std::ostream& os, const std::vector<T>& v)
{
    copy(v.begin(), v.end(), std::ostream_iterator<T>(os, " "));
    return os;
}

inline std::string human_readable_count(uint64_t bytes, bool use_si = false,
                                        std::string unit_string = "B")
{
    uint64_t unit = use_si ? 1000 : 1024;

    if (bytes < unit)
        return boost::lexical_cast<std::string>(bytes) + " " + unit_string;

    uint32_t exponent = static_cast<uint32_t>(std::log(bytes) / std::log(unit));

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
