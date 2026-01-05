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
#include "allocators/arena_allocator.h"
#include <vector>
#include <cstring>

using namespace coll;

class MockFallbackAllocator : public IAllocator
{
public:
    std::vector<void*> allocatedBlocks;
    std::vector<void*> freedBlocks;
    bool shouldFail = false;

    SAllocResult alloc(byte_size bytes, align a) override
    {
        if (shouldFail)
            return {nullptr, 0};

        const byte_size correctedSize = a.round_up(bytes);
        void* ptr = std::malloc(correctedSize);
        allocatedBlocks.push_back(ptr);
        return {ptr, correctedSize};
    }

    void free(void* block) override
    {
        freedBlocks.push_back(block);
        std::free(block);
    }

    byte_size tryExpand(byte_size bytes, void*) override { return 0; }
};

TEST_CASE("ArenaAllocator constructors", "[ArenaAllocator][constructors]")
{
    MockFallbackAllocator fallback;
    uint8_t buffer[1024] = {0};

    SECTION("Constructor with backing buffer")
    {
        span<uint8_t> backing(buffer, 1024);
        ArenaAllocator arena(backing, fallback);

        // No allocation from fallback
        REQUIRE(fallback.allocatedBlocks.empty());
    }

    SECTION("Constructor with size allocates from fallback")
    {
        {
            ArenaAllocator arena(512, fallback);
            REQUIRE(fallback.allocatedBlocks.size() == 1);
        }
        REQUIRE(fallback.freedBlocks.size() == 1);
        CHECK(fallback.allocatedBlocks.front() == fallback.freedBlocks.front());
    }

    SECTION("Constructor with size fails when fallback fails")
    {
        fallback.shouldFail = true;
        REQUIRE_THROWS_AS(ArenaAllocator(512, fallback), std::bad_alloc);
    }
}

TEST_CASE("ArenaAllocator allocation", "[ArenaAllocator][alloc]")
{
    constexpr size_t arenaSize = 1024;
    MockFallbackAllocator fallback;
    uint8_t buffer[arenaSize] = {0};
    span<uint8_t> backing(buffer, arenaSize);
    ArenaAllocator arena(backing, fallback);

    SECTION("Small aligned allocation fits in arena")
    {
        auto result1 = arena.alloc(16, align::system());
        REQUIRE(result1.buffer != nullptr);
        REQUIRE(result1.bytes >= 16);

        auto result2 = arena.alloc(16, align::system());
        REQUIRE(result2.buffer != nullptr);
        REQUIRE(result2.bytes >= 16);
        REQUIRE(result2.buffer > result1.buffer);
    }

    SECTION("Large allocation exceeds arena, uses fallback")
    {
        arena.alloc(900, align::system());
        auto result = arena.alloc(200, align::system());

        REQUIRE(result.buffer != nullptr);
        REQUIRE(fallback.allocatedBlocks.size() == 1);
    }

    SECTION("Alignment padding is correctly calculated")
    {
        // Allocate with 8-byte alignment
        auto result1 = arena.alloc(1, align::from_bytes(8));
        REQUIRE(reinterpret_cast<uintptr_t>(result1.buffer) % 8 == 0);

        auto result2 = arena.alloc(1, align::from_bytes(8));
        REQUIRE(reinterpret_cast<uintptr_t>(result2.buffer) % 8 == 0);
        REQUIRE(result2.buffer > result1.buffer);
    }

    SECTION("Exact arena exhaustion")
    {
        const size_t blockSize = align::system().bytes();
        const size_t prevBlockSize = arenaSize - blockSize;

        arena.alloc(prevBlockSize, align::system());
        auto result = arena.alloc(blockSize, align::system());
        REQUIRE(result.buffer != nullptr);
        REQUIRE(fallback.allocatedBlocks.empty()); // Still fits with padding
    }
}

TEST_CASE("ArenaAllocator destructor", "[ArenaAllocator][destructor]")
{
    MockFallbackAllocator fallback;

    SECTION("Owned buffer is freed on destruction")
    {
        {
            ArenaAllocator arena(512, fallback);
        }
        REQUIRE(fallback.freedBlocks.size() == 1);
    }

    SECTION("Non-owned buffer is not freed")
    {
        uint8_t buffer[1024] = {0};
        span<uint8_t> backing(buffer, 1024);
        {
            ArenaAllocator arena(backing, fallback);
        }
        REQUIRE(fallback.freedBlocks.empty());
    }
}

TEST_CASE("ArenaAllocator free behavior", "[ArenaAllocator][free]")
{
    MockFallbackAllocator fallback;
    uint8_t buffer[1024] = {0};
    span<uint8_t> backing(buffer, 1024);
    ArenaAllocator arena(backing, fallback);

    SECTION("Free arena memory does nothing")
    {
        auto arenaResult = arena.alloc(16, align::system());
        arena.free(arenaResult.buffer);
        // No fallback free called
        REQUIRE(fallback.freedBlocks.empty());
    }

    SECTION("Free fallback memory delegates to fallback")
    {
        auto fallbackResult = fallback.alloc(16, align::system());
        arena.free(fallbackResult.buffer);
        REQUIRE(fallback.freedBlocks.size() == 1);
        REQUIRE(fallback.freedBlocks[0] == fallbackResult.buffer);
    }
}

TEST_CASE("ArenaAllocator tryExpand", "[ArenaAllocator][tryExpand]")
{
    MockFallbackAllocator fallback;
    uint8_t buffer[1024] = {0};
    span<uint8_t> backing(buffer, 1024);
    ArenaAllocator arena(backing, fallback);

    SECTION("tryExpand always returns 0")
    {
        auto result = arena.alloc(16, align::system());
        REQUIRE(arena.tryExpand(32, result.buffer) == 0);
        REQUIRE(arena.tryExpand(0, nullptr) == 0);
    }
}

TEST_CASE("ArenaAllocator edge cases", "[ArenaAllocator][edge]")
{
    MockFallbackAllocator fallback;
    uint8_t buffer[64] = {0};
    span<uint8_t> smallBacking(buffer, 64);
    ArenaAllocator arena(smallBacking, fallback);

    SECTION("Zero byte allocation")
    {
        auto result = arena.alloc(0, align::system());
        REQUIRE(result.buffer != nullptr);
        REQUIRE(result.bytes == 0);
    }

    SECTION("Maximum alignment")
    {
        auto result = arena.alloc(8, align::from_bytes(32));
        REQUIRE(reinterpret_cast<uintptr_t>(result.buffer) % 32 == 0);
    }

    SECTION("Arena buffer exhaustion with alignment")
    {
        arena.alloc(32, align::from_bytes(32));
        arena.alloc(16, align::from_bytes(32));
        auto result = arena.alloc(8, align::from_bytes(8));
        REQUIRE(fallback.allocatedBlocks.size() == 1);
    }
}
