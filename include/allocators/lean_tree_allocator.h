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
 * @brief LeanTreeAllocator: Allocator based un Buddy Allocator. With very low fixed memory overhead.
 *
 */

namespace coll
{
class LeanTreeAllocator : public IAllocator
{
public:
    struct Parameters
    {
        Power2 basicBlockSize = Power2::round_up(16);
        Power2 totalSize = Power2::round_up(64 * 1024);
        Power2 maxAllocSize = Power2::round_up(8 * 1024);
        align a = align::system();
    };

    struct Stats
    {
        byte_size totalBytes = 0;
        byte_size bytesUsed = 0;
        byte_size largestFreeBlock = 0;
    };

    struct Limits
    {
    };

    LeanTreeAllocator(IAllocator& backing, const Parameters& params = Parameters());
    ~LeanTreeAllocator();

    // IAllocator implementation
    SAllocResult alloc(byte_size bytes, align a) override;
    byte_size tryExpand(byte_size bytes, void*) override;
    void free(void*) override;
    ////////

    Stats stats() const { return m_stats; }
    Parameters params() const { return m_header->params; }

    bool validate() const;
    // void dumpToCsv(std::ostream& csv, char separator = ';') const;

private:
    struct SHeader
    {
        uint8_t* data;
        uint8_t** levels;
        Parameters params;
    };

    SAllocResult topLevelAlloc(Power2 basicBlocks);
    Power2 topLevelBlocksCount() const;
    uint8_t topLevel() const;
    uint8_t* allocAtLevel(uint8_t level, count_t index, Power2 basicBlocks);
    count_t selectFittingChild(uint8_t level, count_t index, Power2 basicBlocks);
    bool getBitLevelValue(uint8_t level, count_t index)const;
    void setBitLevelValue(uint8_t level, count_t index, bool value);

    void preSplitCheck(uint8_t level, count_t index);
    void setUsedBits(count_t index, Power2 size);
    void setSolidBit(uint8_t level, count_t index);
    count_t lowerLevelsLfb(uint8_t level, count_t index) const;
    void updateLargestFreeBlock(uint8_t level, count_t index);

    void setupHeader(uint8_t* rawMemory, const Parameters& params);
    void allocMetadata(byte_size size);
    void freeAtBlock(count_t basicBlockIndex, uint8_t level);

    IAllocator& m_backing;
    SHeader* m_header;
    Stats m_stats;
};
} // namespace coll