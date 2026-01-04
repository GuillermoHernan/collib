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
#include "../span.h"

namespace coll
{

/**
 * @brief Arena memory allocator with fallback support
 *
 * # Overview
 * ArenaAllocator provides fast, contiguous memory allocation within a fixed-size
 * backing buffer (arena). Once the arena is exhausted, allocations automatically
 * fall back to a secondary IAllocator. Perfect for reducing fragmentation in
 * short-lived scopes with known upper memory bounds.
 *
 * # Key Characteristics
 * ✅ **Fast**: No per-allocation bookkeeping, just bump pointer
 * ✅ **Cache-friendly**: Contiguous allocations
 * ✅ **Deterministic**: Predictable allocation times (except fallback)
 * ❌ **No individual arena frees**: Arena memory freed on destruction
 * ❌ **Fixed capacity**: Exceeds → fallback allocator
 *
 * # Constructor Parameters
 * ```
 * ArenaAllocator(span<uint8_t> backingBuffer, IAllocator& fallback)
 * ArenaAllocator(byte_size size, IAllocator& fallback)
 * ```
 * **backingBuffer**: Pre-allocated memory buffer (stack/global)
 *  - **NOT freed** by ArenaAllocator destructor
 *  - Must be zero-initialized and valid for arena lifetime
 *
 * **size**: Arena capacity in bytes
 *  - **Allocated from fallback** during construction
 *  - **Automatically freed** by destructor
 *
 * **fallback**: IAllocator& for overflow allocations
 *  - **Required** for both constructors
 *  - **Owned buffer** allocations delegate to fallback
 *
 * # Typical Use Cases ✓
 * ```
 * // External buffer (no ownership)
 * uint8_t buffer[131072] = {0};
 * ArenaAllocator arena(span(buffer), fallback);
 *
 * // Owned buffer (auto-free)
 * ArenaAllocator arena(131072, fallback);
 * ```
 *
 * # When NOT to use ❌
 * - Long-lived objects requiring individual deallocation
 * - Unpredictable memory growth (favor fallback-heavy allocators)
 * - Frequent small allocations after arena exhaustion
 *
 * # Memory Management Rules
 * - **Arena allocations**: free() is no-op (memory freed on destruction)
 * - **Fallback allocations**: free() delegates to fallback allocator
 * - **External buffers**: User responsibility to manage lifetime
 *
 * # Performance Tips
 * - Size arenas to 80-90% of expected peak usage
 * - Use align::system() for generic allocations
 * - Place in scopes matching allocation lifetime
 */

class ArenaAllocator : public IAllocator
{
public:
    ArenaAllocator(span<uint8_t> backingBuffer, IAllocator& fallback);
    ArenaAllocator(byte_size size, IAllocator& fallback);
    ~ArenaAllocator();

    // IAllocator implementation
    SAllocResult alloc(byte_size bytes, align a) override;
    byte_size tryExpand(byte_size bytes, void*) override;
    void free(void*) override;

private:
    IAllocator& m_fallback;
    span<uint8_t> m_buffer;
    byte_size m_usedBytes;
    bool m_ownedBuffer;
};
} // namespace coll