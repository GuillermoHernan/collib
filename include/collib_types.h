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
#include <limits>
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

    constexpr static align from_log2(uint8_t x)
    {
        align a;
        a.m_logSize = x;

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

    constexpr align operator<<(uint8_t offset) const
    {
        align result(*this);
        result.m_logSize += offset;
        return result;
    }

    constexpr align operator>>(uint8_t offset) const
    {
        align result(*this);
        result.m_logSize -= offset;
        return result;
    }

    constexpr byte_size mask() const { return (~byte_size(0)) << m_logSize; }
    constexpr uint8_t log2Size() const { return m_logSize; }

    bool isAligned(const void* ptr) const { return isAligned(reinterpret_cast<uintptr_t>(ptr)); }
    bool isAligned(byte_size size) const { return (size & mask()) == size; }

    template <class T>
    T* apply(T* input) const
    {
        align effective = align::of<T>();

        if (effective < *this)
            effective = *this;

        const byte_size pad = effective.padding<T>(input);

        uint8_t* bytes = reinterpret_cast<uint8_t*>(input);
        return reinterpret_cast<T*>(bytes + pad);
    }

    template <class T>
    constexpr byte_size padding(const T* ptr)
    {
        const uintptr_t address = reinterpret_cast<uintptr_t>(ptr);

        return round_up(address) - address;
    }

    constexpr byte_size round_down(byte_size size) const { return size & mask(); }
    constexpr byte_size round_up(byte_size size) const { return round_down(size + (~mask())); }

private:
    uint8_t m_logSize = 0;
};

/**
 * @brief Power2: Represents exact powers of 2 (2^n where n ∈ [0,255])
 *        Optimized mathematical and bitwise operations for power-of-2 values.
 */
class Power2
{
public:
    Power2() = default;

    /// Creates the smallest power of 2 >= x (round up to power of 2)
    constexpr static Power2 from_value(byte_size x)
    {
        if (x <= 1)
            return Power2(0);
        --x;
        const uint8_t totalBits = uint8_t(sizeof(x) * 8);
        Power2 p;
        p.m_log2 = totalBits - std::countl_zero(x);
        return p;
    }

    /// Creates exactly 2^log2
    constexpr static Power2 from_log2(uint8_t log2)
    {
        Power2 p;
        p.m_log2 = log2;
        return p;
    }

    /// Saturación en conversión a valor numérico
    constexpr byte_size value() const
    {
        if (m_log2 >= 64)
            return std::numeric_limits<byte_size>::max();
        else
            return byte_size(1) << m_log2;
    }

    constexpr uint8_t log2() const { return m_log2; }

    /// Relative level: log2(this) - log2(base)
    constexpr int relative_level(const Power2& base) const { return int(m_log2) - int(base.m_log2); }

    constexpr Power2 parent() const { return Power2(m_log2 > 0 ? m_log2 - 1 : 0); }
    constexpr Power2 child() const { return Power2(m_log2 + 1); }

    constexpr Power2 operator<<(int shift) const { return Power2(uint8_t(m_log2 + shift)); }
    constexpr Power2 operator>>(int shift) const
    {
        return Power2(uint8_t(m_log2 > shift ? m_log2 - shift : 0));
    }

    constexpr Power2 operator*(const Power2& other) const
    {
        return Power2(uint8_t(m_log2 + other.m_log2));
    }
    constexpr Power2 operator/(const Power2& other) const
    {
        return Power2(uint8_t(m_log2 > other.m_log2 ? m_log2 - other.m_log2 : 0));
    }

    constexpr Power2& operator*=(const Power2& other)
    {
        m_log2 = uint8_t(m_log2 + other.m_log2);
        return *this;
    }
    constexpr Power2& operator/=(const Power2& other)
    {
        m_log2 = uint8_t(m_log2 > other.m_log2 ? m_log2 - other.m_log2 : 0);
        return *this;
    }

    constexpr auto operator<=>(const Power2&) const = default;

private:
    constexpr Power2(uint8_t log2)
        : m_log2(log2)
    {
    }
    uint8_t m_log2 = 0;
};

} // namespace coll