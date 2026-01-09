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

#include "allocator.h"

#include "collib_version.h"
#include "bmap.h"
#include "darray.h"

#include <iostream>
#include <functional>
#include <malloc.h>

namespace coll
{
thread_local darray<IAllocator*> tl_defaultAllocators;
thread_local darray<IAllocLogger*> tl_loggers;

// TODO: Make public??
class AtExit
{
public:
    AtExit(std::function<void()>&& fn)
        : m_fn(std::move(fn))
    {
    }

    ~AtExit() { m_fn(); }

private:
    std::function<void()> m_fn;
};

struct MallocAllocator : public IAllocator
{
    SAllocResult alloc(byte_size bytes, align a) override
    {
        SAllocResult result {malloc(bytes), bytes};
        AllocLogger::instance().alloc(*this, bytes, bytes, result.buffer, a);
        return result;
    }

    byte_size tryExpand(byte_size requestedBytes, void* buffer) override
    {
        AllocLogger::instance().tryExpand(*this, requestedBytes, 0, buffer);
        return 0;
    }

    void free(void* buffer) override
    {
        AllocLogger::instance().free(*this, buffer);
        ::free(buffer);
    }
};

IAllocator& defaultAllocator()
{
    static MallocAllocator sillyMallocAllocator;

    if (tl_defaultAllocators.empty())
        return sillyMallocAllocator;
    else
        return *tl_defaultAllocators.back();
}

struct SAllocationKey
{
    const IAllocator* allocator;
    const void* buffer;

    auto operator<=>(const SAllocationKey&) const = default;
};

struct DebugLogSink::SInternal
{
    bmap<SAllocationKey, byte_size, 32> allocations;
};

DebugLogSink::DebugLogSink()
    : m_alloc(defaultAllocator())
    , m_int(create<SInternal>(m_alloc))
{
}

DebugLogSink::~DebugLogSink() { destroy(m_alloc, m_int); }

void DebugLogSink::alloc(
    const IAllocator& alloc,
    byte_size requestedBytes,
    byte_size allocBytes,
    const void* buffer,
    align a
)
{
    m_int->allocations.insert_or_assign(SAllocationKey {&alloc, buffer}, requestedBytes);
}

void DebugLogSink::tryExpand(
    const IAllocator& alloc,
    byte_size requestedBytes,
    byte_size allocBytes,
    const void* buffer
)
{
    if (requestedBytes == 0)
        return;

    m_int->allocations[SAllocationKey {&alloc, buffer}] = allocBytes;
}

void DebugLogSink::free(const IAllocator& alloc, const void* buffer)
{
    if (buffer == nullptr)
        return;

    m_int->allocations.erase(SAllocationKey {&alloc, buffer});
}

count_t DebugLogSink::liveAllocationsCount() const { return m_int->allocations.size(); }

std::ostream& DebugLogSink::reportLiveAllocations(std::ostream& os) const
{
    // CSV header
    os << "address;size_bytes;allocator\n";

    for (const auto& [key, size] : m_int->allocations)
    {
        os << "0x" << std::hex << reinterpret_cast<uintptr_t>(key.buffer);
        os << std::dec << "; " << size;
        os << "; 0x" << std::hex << reinterpret_cast<uintptr_t>(key.allocator);
        os << std::dec << "\n";
    }

    return os;
}

AllocatorHolder::AllocatorHolder(IAllocator& alloc)
    : m_alloc(&alloc)
{
    m_position = tl_defaultAllocators.size();
    tl_defaultAllocators.push_back(m_alloc);
}

void AllocatorHolder::pop()
{
    if (m_alloc != nullptr)
    {
        if (m_position < tl_defaultAllocators.size())
            tl_defaultAllocators[m_position] = nullptr;

        while (!tl_defaultAllocators.empty() && tl_defaultAllocators.back() == nullptr)
            tl_defaultAllocators.pop_back();

        m_alloc = nullptr;
    }
}

AllocLoggerHolder::AllocLoggerHolder(IAllocLogger& logSink)
{
    tl_loggers.push_back(&logSink);
    m_logSink = &logSink;
}

void AllocLoggerHolder::pop()
{
    if (m_logSink != nullptr)
    {
        for (count_t i = 0; i < tl_loggers.size(); ++i)
        {
            if (auto* sink = tl_loggers[i]; sink == m_logSink)
            {
                tl_loggers.erase(i);
                break;
            }
        }

        m_logSink = nullptr;
    }
}

void AllocLogger::alloc(
    const IAllocator& allocator,
    byte_size requestedBytes,
    byte_size allocBytes,
    const void* buffer,
    align a
)
{
    if (m_recursiveGuard)
        return;

    m_recursiveGuard = true;
    AtExit ex([this]() { m_recursiveGuard = false; });

    for (auto* sink : tl_loggers)
        sink->alloc(allocator, requestedBytes, allocBytes, buffer, a);
}

void AllocLogger::tryExpand(
    const IAllocator& allocator,
    byte_size requestedBytes,
    byte_size allocBytes,
    const void* buffer
)
{
    if (m_recursiveGuard)
        return;

    m_recursiveGuard = true;
    AtExit ex([this]() { m_recursiveGuard = false; });

    for (auto* sink : tl_loggers)
        sink->tryExpand(allocator, requestedBytes, allocBytes, buffer);
}

void AllocLogger::free(const IAllocator& allocator, const void* buffer)
{
    if (m_recursiveGuard)
        return;

    m_recursiveGuard = true;
    AtExit ex([this]() { m_recursiveGuard = false; });

    for (auto* sink : tl_loggers)
        sink->free(allocator, buffer);
}

AllocLogger& AllocLogger::instance()
{
    static AllocLogger logger;
    return logger;
}

} // namespace coll