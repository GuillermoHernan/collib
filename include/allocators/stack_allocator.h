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