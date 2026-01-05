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

#include "../allocator.h"
#include <iosfwd>

/**
 * @brief StackAllocator: fast LIFO arena allocator for temporary data
 *
 * DESIGN:
 *  Multiple fixed-size blocks with LIFO allocation within each block.
 *  Each block layout: [Header][Data area growing upwards][Metadata chunks growing downwards]
 *
 * ALLOCATION POLICY:
 *  - O(1) amortized: try current block, allocate new block if no space
 *  - Block sizes grow progressively: minBlockSize → maxBlockSize
 *  - New block size = max(minBlockSize, previous total size, allocationNeed)
 *  - LIFO frees auto-compact: freeing top chunks can reclaim entire blocks
 *
 * USE CASES:
 *  - vectors, dynamic arrays, stacks and other temporary containers
 *  - Frame-local data (particles, render buffers, job data)
 *  - Heap-based replacement for variable-size “stack-like” allocations (VLA-safe)
 *
 * STRENGTHS:
 *  - O(1) alloc/free amortized, no external fragmentation within blocks
 *  - Safe handling of variable-size allocations that do not fit on the call stack
 *  - Auto-compaction under LIFO free patterns
 *  - Per-allocation alignment support (up to 128 bytes)
 *
 * LIMITATIONS:
 *  - Random free order causes internal fragmentation. Non-LIFO patterns waste memory and slow down free,
 *    since the allocator must search for the chunk metadata.
 *  - tryExpand() only works on the top-most chunk in a block.
 *  - Very large allocations are likely to waste memory: they force a dedicated block and may leave
 *    remaining free space in the previous block unused.
 *
 * CONFIGURATION:
 *  StackAllocator alloc(backing, {.minBlockSize = 256, .maxBlockSize = 1_MB});
 *
 *  backing:      Backing allocator used to acquire and release blocks.
 *  minBlockSize: Minimum block size. The first block will have at least this size.
 *  maxBlockSize: Maximum block size. Once reached, new blocks will not grow beyond this.
 *
 *  Block growth policy:
 *    Each new block will be as large as the sum of all previously allocated blocks, capped at
 * maxBlockSize. Exception: if a single allocation bigger than maxBlockSize is requested, a dedicated
 * block large enough to hold that allocation will be obtained from the backing allocator.
 *
 * DEBUG:
 *  - validate()        → checks internal invariants
 *  - dumpToCsv(stream) → dumps Block,Offset,Size,Address,Status
 *  - stats()           → reports allocCount, blockCount, totalMemory
 *
 * TESTED:
 *  10K+ random alloc/free operations plus 5K memory-integrity stress patterns.
 */

namespace coll
{
class StackAllocator : public IAllocator
{
public:
    struct Parameters
    {
        count_t minBlockSize = 256;
        count_t maxBlockSize = 1024 * 1024;
    };

    struct Stats
    {
        byte_size totalMemory = 0;
        count_t allocCount = 0;
        count_t blockCount = 0;
    };

    struct Limits
    {
        constexpr static count_t maxAllocSize = 0x8000'0000;
        constexpr static count_t maxBlockSize = 0x0800'0000;
        constexpr static count_t minBlockSize = 0x20;
        constexpr static align minAlign = align::system();
        constexpr static align maxAlign = align::system() << 7;
    };

    StackAllocator(IAllocator& backing, const Parameters& params = Parameters());
    ~StackAllocator();

    // IAllocator implementation
    SAllocResult alloc(byte_size bytes, align a) override;
    byte_size tryExpand(byte_size bytes, void*) override;
    void free(void*) override;
    ////////

    Stats stats() const { return m_stats; }
    const Parameters& params() const { return m_params; }

    bool validate() const;
    void dumpToCsv(std::ostream& csv, char separator = ';') const;

private:
    struct BlockHeader;

    void pushNewBlock(byte_size allocSize, align a);
    void cleanAfterFree();

    IAllocator& m_backing;
    Parameters m_params;
    Stats m_stats;
    BlockHeader* m_firstBlock = nullptr;
};
} // namespace coll