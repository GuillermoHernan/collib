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
