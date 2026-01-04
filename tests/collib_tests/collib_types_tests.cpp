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

TEST_CASE("align_fix_size", "[align][fix_size]")
{
    align a4 = align::from_bytes(4);

    CHECK(a4.fix_size(0) == 0);
    CHECK(a4.fix_size(1) == 4);
    CHECK(a4.fix_size(3) == 4);
    CHECK(a4.fix_size(4) == 4);
    CHECK(a4.fix_size(5) == 8);
    CHECK(a4.fix_size(7) == 8);
    CHECK(a4.fix_size(8) == 8);

    CHECK(align::from_bytes(1).fix_size(0) == 0);
    CHECK(align::from_bytes(16).fix_size(15) == 16);
    CHECK(align::from_bytes(16).fix_size(17) == 32);
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
