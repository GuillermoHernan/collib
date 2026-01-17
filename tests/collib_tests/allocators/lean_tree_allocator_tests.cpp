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
#include "allocators/lean_tree_allocator.h"
#include <vector>
#include <cstring>
#include <iostream>

using namespace coll;

// Simple backing allocator for controlled behavior in tests.
class DummyAllocator : public IAllocator
{
public:
    std::vector<void*> allocatedBlocks;
    std::vector<void*> freedBlocks;
    bool shouldFail = false;

    SAllocResult alloc(byte_size bytes, align a) override
    {
        if (shouldFail)
            return { nullptr, 0 };

        const byte_size corrected = a.round_up(bytes);
        void* ptr = std::malloc(corrected);
        allocatedBlocks.push_back(ptr);
        return { ptr, corrected };
    }

    void free(void* p) override
    {
        freedBlocks.push_back(p);
        std::free(p);
    }

    byte_size tryExpand(byte_size, void*) override { return 0; }
};

// -------------------------------------------------------------
//  Construction
// -------------------------------------------------------------
TEST_CASE("LeanTreeAllocator construction", "[allocator][lean_tree][construction]")
{
    DummyAllocator backing;
    LeanTreeAllocator::Parameters params;
    LeanTreeAllocator allocator(backing, params);

    auto stats = allocator.stats();
    auto p = allocator.params();

    // Verify total bytes and size consistency
    REQUIRE(stats.totalBytes == p.totalSize.value());
    REQUIRE(stats.bytesUsed == 0);

    // Backing allocator should have allocated (internal metadata)
    REQUIRE(backing.allocatedBlocks.size() == 1);
    REQUIRE(backing.freedBlocks.empty());
}

// -------------------------------------------------------------
//  Basic allocation / free
// -------------------------------------------------------------
TEST_CASE("LeanTreeAllocator alloc and free", "[allocator][lean_tree][alloc]")
{
    DummyAllocator backing;
    LeanTreeAllocator allocator(backing);

    SECTION("Basic allocations increase bytesUsed")
    {
        auto s0 = allocator.stats();
        auto r1 = allocator.alloc(64, align::system());
        REQUIRE(r1.buffer != nullptr);
        auto s1 = allocator.stats();
        CHECK(s1.bytesUsed > s0.bytesUsed);
        allocator.dumpToCsv(std::cerr);

        auto r2 = allocator.alloc(128, align::system());
        REQUIRE(r2.buffer != nullptr);
        auto s2 = allocator.stats();
        CHECK(s2.bytesUsed > s1.bytesUsed);
        std::cerr << std::endl;
        allocator.dumpToCsv(std::cerr);

        allocator.free(r1.buffer);
        auto s3 = allocator.stats();
        CHECK(s3.bytesUsed < s2.bytesUsed);
        std::cerr << std::endl;
        allocator.dumpToCsv(std::cerr);
    }

    SECTION("Multiple blocks have different addresses")
    {
        auto a = allocator.alloc(32, align::system());
        auto b = allocator.alloc(32, align::system());
        REQUIRE(a.buffer != nullptr);
        REQUIRE(b.buffer != nullptr);
        CHECK(a.buffer != b.buffer);
    }
}

// -------------------------------------------------------------
//  Out-of-range and error cases
// -------------------------------------------------------------
TEST_CASE("LeanTreeAllocator allocation limits", "[allocator][lean_tree][limits]")
{
    DummyAllocator backing;
    LeanTreeAllocator allocator(backing);

    const auto params = allocator.params();

    SECTION("Allocation larger than maxAllocSize fails")
    {
        auto result = allocator.alloc(params.maxAllocSize.value() * 2, align::system());
        CHECK(result.buffer == nullptr);
        CHECK(result.bytes == 0);
    }
}

TEST_CASE("LeanTreeAllocator free error cases", "[allocator][lean_tree][free_errors]")
{
    DummyAllocator backing;
    LeanTreeAllocator allocator(backing);
    auto valid = allocator.alloc(64, align::system());

    SECTION("Freeing nullptr throws runtime_error")
    {
        REQUIRE_THROWS_AS(allocator.free(nullptr), std::runtime_error);
    }

    SECTION("Freeing pointer outside managed area throws")
    {
        uint8_t buffer;
        REQUIRE_THROWS_AS(allocator.free(&buffer), std::runtime_error);
    }

    SECTION("Freeing non-aligned pointer throws")
    {
        auto ptr = reinterpret_cast<uint8_t*>(valid.buffer);
        REQUIRE_THROWS_AS(allocator.free(ptr + 1), std::runtime_error);
    }

    allocator.free(valid.buffer);
}

// -------------------------------------------------------------
//  Statistics
// -------------------------------------------------------------
TEST_CASE("LeanTreeAllocator statistics update", "[allocator][lean_tree][stats]")
{
    DummyAllocator backing;
    LeanTreeAllocator allocator(backing);

    auto initial = allocator.stats();
    CHECK(initial.bytesUsed == 0);

    auto r1 = allocator.alloc(128, align::system());
    auto mid = allocator.stats();
    CHECK(mid.bytesUsed >= 128);

    allocator.free(r1.buffer);
    auto end = allocator.stats();
    CHECK(end.bytesUsed < mid.bytesUsed);
}

// -------------------------------------------------------------
//  Fragmentation and reuse
// -------------------------------------------------------------
TEST_CASE("LeanTreeAllocator fragmentation and reuse", "[allocator][lean_tree][reuse]")
{
    DummyAllocator backing;
    LeanTreeAllocator allocator(backing);

    auto a = allocator.alloc(64, align::system());
    auto b = allocator.alloc(64, align::system());
    allocator.free(a.buffer);

    SECTION("Allocator reuses freed space for new allocation")
    {
        auto c = allocator.alloc(32, align::system());
        CHECK(c.buffer != nullptr);
        // At least one freed region should now be reused (same 64‑block area possible).
        CHECK(allocator.stats().bytesUsed >= 64);
    }

    allocator.free(b.buffer);
}

// -------------------------------------------------------------
//  tryExpand (stub)
// -------------------------------------------------------------
TEST_CASE("LeanTreeAllocator tryExpand default behavior", "[allocator][lean_tree][tryExpand]")
{
    DummyAllocator backing;
    LeanTreeAllocator allocator(backing);

    auto result = allocator.alloc(64, align::system());
    CHECK(allocator.tryExpand(128, result.buffer) == 0);
    CHECK(allocator.tryExpand(0, nullptr) == 0);

    allocator.free(result.buffer);
}
