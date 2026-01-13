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
 
#include "pch-collib-tests.h"

#include "collib_types.h"

using namespace coll;

TEST_CASE("align", "[align]")
{
    SECTION("from_bytes creates correct values", "[align][from_bytes]")
    {
        for (size_t i = 0; i < 256; ++i)
        {
            align a = align::from_bytes(i);
            CHECK(a.bytes() >= i);
        }

        CHECK(align::from_bytes(0).bytes() == 1);
        CHECK(align::from_bytes(1).bytes() == 1);
        CHECK(align::from_bytes(2).bytes() == 2);
        CHECK(align::from_bytes(3).bytes() == 4);
        CHECK(align::from_bytes(4).bytes() == 4);
        CHECK(align::from_bytes(11).bytes() == 16);
        CHECK(align::from_bytes(15).bytes() == 16);
        CHECK(align::from_bytes(16).bytes() == 16);
        CHECK(align::from_bytes(17).bytes() == 32);
    }

    SECTION("from_bits creates correct values", "[align][from_bits]")
    {
        for (size_t i = 0; i < 512; ++i)
        {
            align a = align::from_bits(i);
            CHECK(a.bits() >= i);
            CHECK(a.bits() == a.bytes() * 8);
        }

        CHECK(align::from_bits(17).bytes() == 4);
        CHECK(align::from_bits(31).bytes() == 4);
        CHECK(align::from_bits(32).bytes() == 4);
        CHECK(align::from_bits(64).bytes() == 8);
        CHECK(align::from_bits(67).bytes() == 16);
    }
}

TEST_CASE("align_from_bytes", "[align][from_bytes]")
{
    for (size_t i = 0; i < 256; ++i)
    {
        align a = align::from_bytes(i);
        CHECK(a.bytes() >= i);
    }

    CHECK(align::from_bytes(0).bytes() == 1);
    CHECK(align::from_bytes(1).bytes() == 1);
    CHECK(align::from_bytes(2).bytes() == 2);
    CHECK(align::from_bytes(3).bytes() == 4);
    CHECK(align::from_bytes(4).bytes() == 4);
    CHECK(align::from_bytes(11).bytes() == 16);
    CHECK(align::from_bytes(15).bytes() == 16);
    CHECK(align::from_bytes(16).bytes() == 16);
    CHECK(align::from_bytes(17).bytes() == 32);
}

TEST_CASE("align_from_bits", "[align][from_bits]")
{
    for (size_t i = 0; i < 512; ++i)
    {
        align a = align::from_bits(i);
        CHECK(a.bits() >= i);
        CHECK(a.bits() == a.bytes() * 8);
    }

    CHECK(align::from_bits(17).bytes() == 4);
    CHECK(align::from_bits(31).bytes() == 4);
    CHECK(align::from_bits(32).bytes() == 4);
    CHECK(align::from_bits(64).bytes() == 8);
    CHECK(align::from_bits(67).bytes() == 16);
}

TEST_CASE("align_comparisons", "[align][comparisons]")
{
    align a1 = align::from_bytes(1);
    align a2 = align::from_bytes(2);
    align a4 = align::from_bytes(4);
    align a8 = align::from_bytes(8);

    CHECK(a1 == a1);
    CHECK(a1 != a2);
    CHECK(a1 < a2);
    CHECK(a2 < a4);
    CHECK(a4 <= a8);
    CHECK(a8 > a4);
    CHECK(a8 >= a4);
}

TEST_CASE("align_mask", "[align][mask]")
{
    align a1 = align::from_bytes(1);
    align a2 = align::from_bytes(2);
    align a4 = align::from_bytes(4);
    align a6 = align::from_bytes(6);

    CHECK(a1.mask() == ~byte_size(0));
    CHECK(a2.mask() == ~byte_size(0x1));
    CHECK(a4.mask() == ~byte_size(0x3));
    CHECK(a6.mask() == ~byte_size(0x7));
}

TEST_CASE("align_fix_size", "[align][round_up]")
{
    align a4 = align::from_bytes(4);

    CHECK(a4.round_up(0) == 0);
    CHECK(a4.round_up(1) == 4);
    CHECK(a4.round_up(3) == 4);
    CHECK(a4.round_up(4) == 4);
    CHECK(a4.round_up(5) == 8);
    CHECK(a4.round_up(7) == 8);
    CHECK(a4.round_up(8) == 8);

    CHECK(align::from_bytes(1).round_up(0) == 0);
    CHECK(align::from_bytes(16).round_up(15) == 16);
    CHECK(align::from_bytes(16).round_up(17) == 32);
}

TEST_CASE("align_padding", "[align][padding]")
{
    align a8 = align::from_bytes(8);

    CHECK(a8.padding((int*)0x8) == 0);
    CHECK(a8.padding((double*)0x10) == 0);
    CHECK(a8.padding((char*)0x18) == 0);

    CHECK(a8.padding((int*)0x1) == 7);
    CHECK(a8.padding((int*)0x2) == 6);
    CHECK(a8.padding((int*)0x3) == 5);
    CHECK(a8.padding((int*)0x4) == 4);
    CHECK(a8.padding((int*)0x5) == 3);
    CHECK(a8.padding((int*)0x6) == 2);
    CHECK(a8.padding((int*)0x7) == 1);

    align a4 = align::from_bytes(4);
    CHECK(a4.padding((float*)0x2) == 2);
    CHECK(a4.padding((long*)0x6) == 2);

    CHECK(a8.padding((void*)nullptr) == 0);
}

TEST_CASE("power2_from_value", "[power2][round_up]")
{
    CHECK(Power2::round_up(0).log2() == 0);
    CHECK(Power2::round_up(1).log2() == 0);
    CHECK(Power2::round_up(2).log2() == 1);
    CHECK(Power2::round_up(3).log2() == 2);
    CHECK(Power2::round_up(4).log2() == 2);
    CHECK(Power2::round_up(5).log2() == 3);
    CHECK(Power2::round_up(8).log2() == 3);
    CHECK(Power2::round_up(9).log2() == 4);

    SECTION("values round up to next power of 2")
    {
        for (byte_size i = 1; i < 256; ++i)
        {
            Power2 p = Power2::round_up(i);
            CHECK(p.value() >= i);
            CHECK((p.value() & (p.value() - 1)) == 0); // must be power of two
        }
    }
}

TEST_CASE("power2_round_down", "[power2][round_down]")
{
    CHECK(Power2::round_down(0).log2() == 0);
    CHECK(Power2::round_down(1).log2() == 0);
    CHECK(Power2::round_down(2).log2() == 1);
    CHECK(Power2::round_down(3).log2() == 1);
    CHECK(Power2::round_down(4).log2() == 2);
    CHECK(Power2::round_down(5).log2() == 2);
    CHECK(Power2::round_down(7).log2() == 2);
    CHECK(Power2::round_down(8).log2() == 3);
    CHECK(Power2::round_down(9).log2() == 3);
    CHECK(Power2::round_down(15).log2() == 3);
    CHECK(Power2::round_down(16).log2() == 4);
    CHECK(Power2::round_down(17).log2() == 4);

    SECTION("round_down always produces value <= input")
    {
        for (byte_size i = 1; i < 256; ++i)
        {
            Power2 p = Power2::round_down(i);
            CHECK(p.value() <= i);
            CHECK((p.value() & (p.value() - 1)) == 0); // must still be a power of two
        }
    }

    SECTION("round_down matches exact powers")
    {
        for (uint8_t e = 0; e < 16; ++e)
        {
            byte_size v = byte_size(1) << e;
            Power2 p = Power2::round_down(v);
            CHECK(p.log2() == e);
            CHECK(p.value() == v);
        }
    }
}

TEST_CASE("power2_from_log2", "[power2][from_log2]")
{
    Power2 p0 = Power2::from_log2(0);
    Power2 p5 = Power2::from_log2(5);
    Power2 p10 = Power2::from_log2(10);

    CHECK(p0.value() == 1);
    CHECK(p5.value() == (byte_size(1) << 5));
    CHECK(p10.value() == (byte_size(1) << 10));
    CHECK(p10.log2() == 10);
}

TEST_CASE("power2_value_saturation", "[power2][saturation]")
{
    Power2 big = Power2::from_log2(64);
    Power2 huge = Power2::from_log2(80);

    CHECK(big.value() == std::numeric_limits<byte_size>::max());
    CHECK(huge.value() == std::numeric_limits<byte_size>::max());
}

TEST_CASE("power2_parent_child", "[power2][hierarchy]")
{
    Power2 p4 = Power2::from_log2(4);
    Power2 parent = p4.parent();
    Power2 child = p4.child();

    CHECK(parent.log2() == 3);
    CHECK(child.log2() == 5);
    CHECK(parent.value() == (p4.value() >> 1));
    CHECK(child.value() == (p4.value() << 1));
}

TEST_CASE("power2_relative_level", "[power2][relative]")
{
    Power2 p8 = Power2::from_log2(3);
    Power2 p64 = Power2::from_log2(6);

    CHECK(p8.relative_level(p64) == -3);
    CHECK(p64.relative_level(p8) == 3);
}

TEST_CASE("power2_bit_shifts", "[power2][shift]")
{
    Power2 p2 = Power2::from_log2(1);
    CHECK((p2 << 2).log2() == 3);
    CHECK((p2 >> 1).log2() == 0);

    Power2 p8 = Power2::from_log2(3);
    CHECK((p8 >> 4).log2() == 0);
    CHECK((p8 << 1).value() == 16);
}

TEST_CASE("power2_multiplication_division", "[power2][arithmetic]")
{
    Power2 p4 = Power2::from_log2(2);
    Power2 p8 = Power2::from_log2(3);

    CHECK((p4 * p8).log2() == 5);
    CHECK((p8 / p4).log2() == 1);
    CHECK((p4 / p8).log2() == 0);

    Power2 p = p4;
    p *= p8;
    CHECK(p.log2() == 5);

    p /= p8;
    CHECK(p.log2() == 2);
}

TEST_CASE("power2_comparisons", "[power2][comparisons]")
{
    Power2 p2 = Power2::from_log2(1);
    Power2 p4 = Power2::from_log2(2);
    Power2 p8 = Power2::from_log2(3);

    CHECK(p2 < p4);
    CHECK(p4 < p8);
    CHECK(p8 > p4);
    CHECK(p8 >= p4);
    CHECK(p4 <= p8);
    CHECK(p2 != p4);
    CHECK(p4 == p4);
}

TEST_CASE("power2_byte_operations", "[power2][mod_div]")
{
    Power2 p4 = Power2::from_log2(2);
    Power2 p8 = Power2::from_log2(3);

    CHECK((byte_size(10) % p4) == 2); // 10 mod 4 = 2
    CHECK((byte_size(8) % p8) == 0);
    CHECK((byte_size(15) % p8) == 7);

    CHECK((byte_size(16) / p4) == 4);
    CHECK((byte_size(32) / p8) == 4);
    CHECK((byte_size(9) / p8) == 1);
}
