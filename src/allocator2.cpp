#include "allocator2.h"

#include "collib_version.h"
#include "bmap.h"
#include "darray.h"

#include <iostream>
#include <malloc.h>

namespace coll
{
thread_local darray<IAllocator*> tl_defaultAllocators;

struct MallocAllocator : public IAllocator
{
    SAllocResult alloc(byte_size bytes, align) override { return {malloc(bytes), bytes}; }

    byte_size tryExpand(byte_size, void*) override { return 0; }

    void free(void* buffer) override { ::free(buffer); }
};

IAllocator& defaultAllocator()
{
    static MallocAllocator sillyMallocAllocator;

    if (tl_defaultAllocators.empty())
        return sillyMallocAllocator;
    else
        return *tl_defaultAllocators.back();
}

struct DebugAllocator::SInternal
{
    bmap<void*, byte_size, 32> allocations;
};

DebugAllocator::DebugAllocator(IAllocator& alloc)
    : m_int(create<SInternal>(alloc))
    , m_alloc(alloc)
{
}

DebugAllocator::~DebugAllocator() { destroy(m_alloc, m_int); }

SAllocResult DebugAllocator::alloc(byte_size bytes, align a)
{
    SAllocResult result = m_alloc.alloc(bytes, a);

    if (result.buffer != nullptr)
        m_int->allocations.insert_or_assign(result.buffer, result.bytes);

    return result;
}

byte_size DebugAllocator::tryExpand(byte_size bytes, void* ptr)
{
    const byte_size newSize = m_alloc.tryExpand(bytes, ptr);

    if (newSize > bytes)
        m_int->allocations[ptr] = newSize;

    return newSize;
}

void DebugAllocator::free(void* ptr)
{
    if (ptr == nullptr)
        return;

    m_alloc.free(ptr);
    m_int->allocations.erase(ptr);
}

count_t DebugAllocator::liveAllocationsCount() const { return m_int->allocations.size(); }

std::ostream& DebugAllocator::reportLiveAllocations(std::ostream& os) const
{
    // CSV header
    os << "address;size_bytes\n";

    for (const auto& [ptr, size] : m_int->allocations)
    {
        os << "0x" << std::hex << reinterpret_cast<uintptr_t>(ptr) << std::dec << ";" << size << "\n";
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

} // namespace coll