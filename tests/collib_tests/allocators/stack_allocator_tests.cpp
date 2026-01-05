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
#include "allocators/stack_allocator.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <memory>
#include <random>
#include <vector>

using namespace coll;

class MockBackingAllocator : public IAllocator
{
public:
    std::vector<void*> allocatedBlocks;
    std::vector<void*> freedBlocks;
    bool shouldFail = false;
    std::vector<SAllocResult> allocResults;

    MockBackingAllocator() = default;

    SAllocResult alloc(byte_size bytes, align a) override
    {
        if (shouldFail)
            return {nullptr, 0};

        const byte_size correctedSize = a.fix_size(bytes);
        void* ptr = std::malloc(correctedSize);
        allocatedBlocks.push_back(ptr);

        // Store actual result for verification
        SAllocResult result = {ptr, correctedSize};
        allocResults.push_back(result);
        return result;
    }

    byte_size tryExpand(byte_size, void*) override { return 0; }

    void free(void* block) override
    {
        freedBlocks.push_back(block);
        std::free(block);
    }

    void clear()
    {
        allocatedBlocks.clear();
        freedBlocks.clear();
        allocResults.clear();
        shouldFail = false;
    }
};

static bool checkAllocator(const StackAllocator& allocator)
{
    if (!allocator.validate())
    {
        WARN("=== STACK ALLOCATOR CORRUPTED ===");
        allocator.dumpToCsv(std::cerr);
        std::cerr << "\n\n";
        return false;
    }

    return true;
}

static bool checkAllocatorEmpty(const StackAllocator& allocator)
{  
    auto finalStats = allocator.stats();

    CHECK(finalStats.allocCount == 0);
    CHECK(finalStats.blockCount == 0);
    CHECK(finalStats.totalMemory == 0);

    // clang-format off
    return checkAllocator(allocator)
        && finalStats.allocCount == 0
        && finalStats.blockCount == 0
        && finalStats.totalMemory == 0;
    // clang-format on
}


TEST_CASE("StackAllocator constructors and destructor", "[StackAllocator][constructors]")
{
    MockBackingAllocator backing;
    StackAllocator::Parameters params {256, 1024};

    SECTION("Constructor initializes correctly")
    {
        StackAllocator allocator(backing, params);
        auto stats = allocator.stats();
        REQUIRE(stats.totalMemory == 0);
        REQUIRE(stats.allocCount == 0);
        REQUIRE(stats.blockCount == 0);
        REQUIRE(allocator.params().minBlockSize == 256);

        REQUIRE(checkAllocator(allocator));
    }

    SECTION("Destructor frees all blocks")
    {
        {
            StackAllocator allocator(backing, params);
            allocator.alloc(100, align::system());
            REQUIRE(checkAllocator(allocator));
        }
        // Backing should have freed all blocks
        REQUIRE(backing.freedBlocks.size() == backing.allocatedBlocks.size());
    }
}

TEST_CASE("StackAllocator basic allocation", "[StackAllocator][alloc]")
{
    MockBackingAllocator backing;
    StackAllocator allocator(backing, {64, 512});

    SECTION("Small allocations create blocks correctly")
    {
        auto result1 = allocator.alloc(16, align::system());
        REQUIRE(result1.buffer != nullptr);
        REQUIRE(result1.bytes >= 16);
        REQUIRE(backing.allocatedBlocks.size() == 1);

        auto stats = allocator.stats();
        REQUIRE(stats.blockCount == 1);
        REQUIRE(stats.allocCount == 1);

        REQUIRE(checkAllocator(allocator));
    }

    SECTION("Multiple allocations in same block")
    {
        allocator.alloc(16, align::system());
        allocator.alloc(16, align::system());

        auto stats = allocator.stats();
        CHECK(stats.blockCount == 1); // Still one block
        CHECK(stats.allocCount == 2);

        allocator.alloc(32, align::system());
        stats = allocator.stats();
        CHECK(stats.blockCount == 2); // New block added.
        CHECK(stats.allocCount == 3);

        REQUIRE(checkAllocator(allocator));
    }
}

TEST_CASE("StackAllocator alignment", "[StackAllocator][alignment]")
{
    MockBackingAllocator backing;
    StackAllocator allocator(backing);

    SECTION("System alignment works")
    {
        auto result = allocator.alloc(16, align::system());
        REQUIRE(reinterpret_cast<uintptr_t>(result.buffer) % align::system().bytes() == 0);

        REQUIRE(checkAllocator(allocator));
    }

    SECTION("Custom alignment works")
    {
        auto result = allocator.alloc(1, align::from_bytes(32));
        REQUIRE(reinterpret_cast<uintptr_t>(result.buffer) % 32 == 0);

        REQUIRE(checkAllocator(allocator));
    }

    SECTION("Alignment creates new block when needed")
    {
        allocator.alloc(1, align::from_bytes(64));
        allocator.alloc(1, align::from_bytes(128)); // Needs new block due to alignment

        auto stats = allocator.stats();
        REQUIRE(stats.blockCount == 2);

        REQUIRE(checkAllocator(allocator));
    }
}

TEST_CASE("StackAllocator free behavior", "[StackAllocator][free]")
{
    MockBackingAllocator backing;
    StackAllocator allocator(backing);

    SECTION("Free non-existing pointer throws")
    {
        REQUIRE_THROWS_AS(allocator.free(nullptr), std::runtime_error);
    }

    SECTION("Free LIFO order compacts correctly")
    {
        auto ptr3 = allocator.alloc(32, align::system()).buffer;
        auto ptr2 = allocator.alloc(32, align::system()).buffer;
        auto ptr1 = allocator.alloc(32, align::system()).buffer;

        allocator.free(ptr1); // Wrong order - marks as free but no compaction
        auto stats1 = allocator.stats();
        REQUIRE(stats1.allocCount == 2); // Still 2 active allocations

        allocator.free(ptr2); // Still wrong order
        auto stats2 = allocator.stats();
        REQUIRE(stats2.allocCount == 1);

        allocator.free(ptr3); // Correct LIFO order - compacts fully
        auto stats3 = allocator.stats();
        REQUIRE(stats3.allocCount == 0);
        REQUIRE(stats3.blockCount == 0); // Block freed completely

        REQUIRE(checkAllocator(allocator));
    }

    SECTION("Free random order - internal fragmentation")
    {
        auto ptr3 = allocator.alloc(32, align::system()).buffer;
        auto ptr2 = allocator.alloc(32, align::system()).buffer;
        allocator.free(ptr2); // Middle allocation freed

        auto stats = allocator.stats();
        REQUIRE(stats.allocCount == 1); // ptr3 still counted
        REQUIRE(stats.blockCount == 1); // Block NOT freed (fragmented)

        REQUIRE(checkAllocator(allocator));
    }
}

TEST_CASE("StackAllocator block sizing", "[StackAllocator][blocks]")
{
    MockBackingAllocator backing;
    StackAllocator allocator(backing, {64, 256});

    SECTION("Progressive block sizing")
    {
        allocator.alloc(50, align::system()); // ~64 bytes block
        allocator.alloc(100, align::system()); // May trigger larger block

        auto stats = allocator.stats();
        REQUIRE(stats.blockCount <= 2);

        REQUIRE(checkAllocator(allocator));
    }

    SECTION("Large allocation respects maxBlockSize")
    {
        StackAllocator::Parameters largeParams {64, 128}; // Small max
        StackAllocator limitedAllocator(backing, largeParams);
        limitedAllocator.alloc(1000, align::system()); // Will be capped

        auto stats = limitedAllocator.stats();
        REQUIRE(stats.blockCount == 1);

        REQUIRE(checkAllocator(allocator));
    }
}

TEST_CASE("StackAllocator tryExpand", "[StackAllocator][tryExpand]")
{
    MockBackingAllocator backing;
    StackAllocator allocator(backing);

    SECTION("Last chunk expands, others fail")
    {
        void* first = allocator.alloc(16, align::system()).buffer;
        void* last = allocator.alloc(32, align::system()).buffer;

        CHECK(allocator.tryExpand(64, first) == 0);
        CHECK(allocator.tryExpand(64, last) == 64);

        REQUIRE(checkAllocator(allocator));
    }

    SECTION("Unknown chunk")
    {
        uint8_t dummyBuffer[128];
        allocator.alloc(16, align::system());
        auto r2 = allocator.alloc(16, align::system());

        // Not present in the allocator.
        REQUIRE(allocator.tryExpand(32, dummyBuffer) == 0);

        REQUIRE(checkAllocator(allocator));
    }

    SECTION("No space available")
    {
        StackAllocator::Parameters tiny {32, 32};
        StackAllocator tinyAllocator(backing, tiny);

        // This will require a block larger than maximum, so it will create a block with just room enough
        // for the allocation. So expansion should fail.
        auto r = tinyAllocator.alloc(32, align::system());

        REQUIRE(tinyAllocator.tryExpand(64, r.buffer) == 0);

        CHECK(checkAllocator(tinyAllocator));
    }

    SECTION("Never shrinks + limits")
    {
        auto r = allocator.alloc(48, align::system());
        REQUIRE(allocator.tryExpand(16, r.buffer) == 48); // Never shrinks
        REQUIRE(allocator.tryExpand(1000, r.buffer) > 48); // Block limited

        REQUIRE(checkAllocator(allocator));
    }
}

TEST_CASE("StackAllocator stats", "[StackAllocator][stats]")
{
    MockBackingAllocator backing;
    StackAllocator allocator(backing);

    SECTION("Stats track external allocations correctly")
    {
        allocator.alloc(16, align::system());
        allocator.alloc(32, align::system());
        allocator.free(allocator.alloc(64, align::system()).buffer);

        auto stats = allocator.stats();
        REQUIRE(stats.allocCount == 2); // External count
        REQUIRE(stats.blockCount >= 1);
        REQUIRE(stats.totalMemory > 0);

        REQUIRE(checkAllocator(allocator));
    }
}

TEST_CASE("StackAllocator edge cases", "[StackAllocator][edge]")
{
    MockBackingAllocator backing;
    StackAllocator allocator(backing);

    SECTION("Zero byte allocation")
    {
        auto result = allocator.alloc(0, align::system());
        REQUIRE(result.buffer != nullptr);
        REQUIRE(result.bytes == 0);

        REQUIRE(checkAllocator(allocator));
    }

    SECTION("Max alignment")
    {
        auto result = allocator.alloc(1, align::from_bytes(64));
        REQUIRE(reinterpret_cast<uintptr_t>(result.buffer) % 64 == 0);

        REQUIRE(checkAllocator(allocator));
    }

    SECTION("Repeated free is caught")
    {
        auto ptr = allocator.alloc(16, align::system()).buffer;
        allocator.free(ptr);
        REQUIRE_THROWS_AS(allocator.free(ptr), std::runtime_error);

        REQUIRE(checkAllocator(allocator));
    }
}

TEST_CASE("StackAllocator stress test", "[StackAllocator][stress]")
{
    MockBackingAllocator backing;
    StackAllocator allocator(backing);

    SECTION("Massive random allocation/free stress")
    {
        static constexpr int numAllocs = 10000;
        static constexpr int maxAllocSize = 256;
        std::vector<void*> allocations;
        allocations.reserve(numAllocs);

        std::mt19937 rng(42);
        std::uniform_int_distribution<int> sizeDist(8, maxAllocSize);

        // PHASE 1: Mass allocations
        for (int i = 0; i < numAllocs; ++i)
        {
            byte_size size = sizeDist(rng);
            auto result = allocator.alloc(size, align::system());
            REQUIRE(result.buffer != nullptr);
            allocations.push_back(result.buffer);

            if (i % 1000 == 0)
            {
                INFO("Allocation " << i);
                REQUIRE(checkAllocator(allocator));
            }
        }
        REQUIRE(checkAllocator(allocator));

        // PHASE 2: random frees
        std::shuffle(allocations.begin(), allocations.end(), rng);
        for (size_t i = 0; i < allocations.size(); ++i)
        {
            allocator.free(allocations[i]);
            if (i % 1000 == 0)
            {
                INFO("Free " << i << "/" << allocations.size());
                REQUIRE(checkAllocator(allocator));
            }
        }

        CHECK(checkAllocatorEmpty(allocator));
    }

    SECTION("LIFO pattern stress - real workload simulation")
    {
        for (int wave = 0; wave < 50; ++wave)
        {
            std::vector<void*> waveAllocs;

            for (int i = 0; i < 200; ++i)
            {
                byte_size size = 16 + (i % 64);
                auto result = allocator.alloc(size, align::system());
                waveAllocs.push_back(result.buffer);
            }
            REQUIRE(checkAllocator(allocator));

            // Free in reverse order (LIFO)
            std::reverse(waveAllocs.begin(), waveAllocs.end());
            for (void* ptr : waveAllocs)
                allocator.free(ptr);

            CHECK(checkAllocatorEmpty(allocator));
        }
    }

    SECTION("Mixed random + LIFO stress")
    {
        std::mt19937 rng(12345);
        std::vector<void*> randomAllocs;

        for (int wave = 0; wave < 10; ++wave)
        {
            std::vector<void*> lifoAllocs;

            for (int i = 0; i < 70; ++i)
            {
                auto r = allocator.alloc(32 + (i % 32), align::system());
                lifoAllocs.push_back(r.buffer);
            }
            REQUIRE(checkAllocator(allocator));

            // LIFO free
            std::reverse(lifoAllocs.begin(), lifoAllocs.end());
            for (void* ptr : lifoAllocs)
                allocator.free(ptr);

            REQUIRE(checkAllocator(allocator));

            // Random allocations
            for (int i = 0; i < 30; ++i)
            {
                byte_size size = 8 + (rng() % 128);
                auto r = allocator.alloc(size, align::system());
                randomAllocs.push_back(r.buffer);
            }
            REQUIRE(checkAllocator(allocator));
        }

        // Final cleanup
        std::shuffle(randomAllocs.begin(), randomAllocs.end(), rng);
        for (void* ptr : randomAllocs)
            allocator.free(ptr);

        CHECK(checkAllocatorEmpty(allocator));
    }
}

// Auxiliary functions for stress tests.
static void fillUniquePattern(void* ptr, byte_size size, int allocId)
{
    std::string pattern = "A" + std::to_string(allocId) + "_";
    std::fill_n(static_cast<char*>(ptr), size, '.');
    std::memcpy(static_cast<char*>(ptr), pattern.c_str(), std::min(pattern.size(), size));
}

static bool verifyContent(void* ptr, const std::string& expected)
{
    const char* mem = static_cast<const char*>(ptr);
    std::string actual(mem, std::min(expected.size(), size_t(16)));
    return actual == expected;
}

TEST_CASE("StackAllocator memory integrity stress", "[StackAllocator][integrity]")
{
    MockBackingAllocator backing;
    StackAllocator allocator(backing);

    static constexpr int numAllocs = 5000;
    static constexpr int maxSize = 128;
    std::vector<std::pair<void*, std::string>> allocations;

    SECTION("Random alloc/free with content verification")
    {
        std::mt19937 rng(4242);
        std::uniform_int_distribution<int> sizeDist(16, maxSize);

        // FASE 1: Alloc + unique content
        for (int i = 0; i < numAllocs; ++i)
        {
            byte_size size = sizeDist(rng);
            auto result = allocator.alloc(size, align::system());
            REQUIRE(result.buffer != nullptr);

            std::string pattern = "A" + std::to_string(i) + "_";
            fillUniquePattern(result.buffer, size, i);
            allocations.emplace_back(result.buffer, pattern);

            if (i % 500 == 0)
            {
                INFO("Filled " << i);
                REQUIRE(checkAllocator(allocator));
            }
        }
        REQUIRE(checkAllocator(allocator));

        // FASE 2: Random free + integrity check
        std::shuffle(allocations.begin(), allocations.end(), rng);
        for (size_t i = 0; i < allocations.size(); ++i)
        {
            void* ptr = allocations[i].first;

            // Verify content before antes de free
            for (size_t j = i + 1; j < allocations.size(); ++j)
            {
                if (!verifyContent(allocations[j].first, allocations[j].second))
                {
                    FAIL("Corruption! alloc " << j << " damaged before free " << i);
                }
            }

            allocator.free(ptr);

            if (i % 500 == 0)
            {
                INFO("Verified " << i << "/" << allocations.size());
                REQUIRE(checkAllocator(allocator));
            }
        }

        CHECK(checkAllocatorEmpty(allocator));
    }
}
