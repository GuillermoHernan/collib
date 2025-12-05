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
