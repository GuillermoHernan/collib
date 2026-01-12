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
        // Note: that align is used for sizes because they need to be power of two.
        // TODO: Write a specific class for power of two number.
        align basicBlockSize = align::from_bytes(16);
        align totalSize = align::from_bytes(64 * 1024);
        align maxAllocSize = align::from_bytes(8 * 1024);
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

    SAllocResult topLevelAlloc(align size);
    align topLevelBlocksCount() const;
    uint8_t topLevel() const;
    uint8_t* allocAtLevel(uint8_t level, count_t index, uint8_t logSize);
    count_t selectFittingChild(uint8_t level, count_t index, uint8_t logSize);

    void preSplitCheck(uint8_t level, count_t index);
    void setUsedBits(count_t index, uint8_t logSize);
    void setSolidBit(uint8_t level, count_t index);
    count_t lowerLevelsLfb(uint8_t level, count_t index) const;
    void updateLargestFreeBlock(uint8_t level, count_t index);

    void setupHeader(uint8_t* rawMemory, const Parameters& params);
    void allocMetadata(byte_size size);

    IAllocator& m_backing;
    SHeader* m_header;
    Stats m_stats;
};
} // namespace coll