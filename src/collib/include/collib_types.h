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

private:
    uint8_t m_logSize = 0;
};
} // namespace coll