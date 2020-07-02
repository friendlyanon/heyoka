// Copyright 2020 Francesco Biscani (bluescarni@gmail.com), Dario Izzo (dario.izzo@gmail.com)
//
// This file is part of the heyoka library.
//
// This Source Code Form is subject to the terms of the Mozilla
// Public License v. 2.0. If a copy of the MPL was not distributed
// with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef HEYOKA_DETAIL_STRING_CONV_HPP
#define HEYOKA_DETAIL_STRING_CONV_HPP

// #include <cstdint>
#include <ios>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>

#include <heyoka/detail/visibility.hpp>

namespace heyoka::detail
{

// Locale-independent to_string()/from_string implementations. See:
// https://stackoverflow.com/questions/1333451/locale-independent-atof
template <typename T>
inline std::string li_to_string(const T &x)
{
    std::ostringstream oss;
    oss.exceptions(std::ios_base::failbit | std::ios_base::badbit);
    oss.imbue(std::locale("C"));
    if constexpr (std::is_floating_point_v<T>) {
        // For floating-point types, make
        // sure we print as many digits as necessary
        // to represent "exactly" the value.
        oss.precision(std::numeric_limits<T>::max_digits10);
    }
    oss << x;
    return oss.str();
}

template <typename T>
inline T li_from_string(const std::string &s)
{
    T out(0);
    std::istringstream iss(s);
    iss.exceptions(std::ios_base::failbit | std::ios_base::badbit);
    iss.imbue(std::locale("C"));
    iss >> out;
    // NOTE: if we did not reach the end of the string,
    // raise an error.
    if (!iss.eof()) {
        throw std::invalid_argument("Error converting the string '" + s + "' to a numerical value");
    }
    return out;
}

} // namespace heyoka::detail

#endif