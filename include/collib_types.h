/*
 * Copyright (c) 2026 Guillermo Hernan Martin
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#include <bit>
#include <compare>
#include <stdint.h>

namespace coll
{
using byte_size = size_t;
using count_t = uint32_t;

// To represent align values, which are power of two byte counts, starting in one;
class align
{
public:
    align() = default;

    constexpr static align from_bytes(byte_size x)
    {
        if (x > 0)
            --x;

        const int totalBits = int(sizeof(x) * 8);

        align a;
        a.m_logSize = totalBits - std::countl_zero(x);

        return a;
    }

    constexpr static align from_bits(byte_size x) { return from_bytes((x + 7) / 8); }
    constexpr static align system() { return from_bytes(sizeof(void*)); }

    template <typename T>
    constexpr static align of()
    {
        return from_bytes(alignof(T));
    }

    template <typename T>
    constexpr static align of(const T&)
    {
        return from_bytes(alignof(T));
    }

    constexpr byte_size bytes() const { return byte_size(1) << m_logSize; }
    constexpr byte_size bits() const { return bytes() * 8; }

    constexpr auto operator<=>(const align&) const = default;

    constexpr byte_size mask() const { return (~byte_size(0)) << m_logSize; }

    template <class T>
    constexpr byte_size padding(const T* ptr)
    {
        const uintptr_t address = reinterpret_cast<uintptr_t>(ptr);

        return fix_size(address) - address;
    }

    constexpr byte_size fix_size(byte_size size)
    {
        const byte_size m = mask();
        return (size + (~m)) & m;
    }

private:
    uint8_t m_logSize = 0;
};
} // namespace coll