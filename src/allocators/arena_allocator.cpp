#include "allocators/arena_allocator.h"

namespace coll
{

ArenaAllocator::ArenaAllocator(span<uint8_t> backingBuffer, IAllocator& fallback)
    : m_fallback(fallback)
    , m_buffer(backingBuffer)
    , m_usedBytes(0)
    , m_ownedBuffer(false)
{
}

ArenaAllocator::ArenaAllocator(byte_size size, IAllocator& fallback)
    : m_fallback(fallback)
    , m_usedBytes(0)
    , m_ownedBuffer(true)
{
    auto [buffer, bufferSize] = fallback.alloc(size, align::system());

    if (buffer == nullptr)
        throw std::bad_alloc();

    m_buffer = span<uint8_t>(reinterpret_cast<uint8_t*>(buffer), count_t(bufferSize));
}

ArenaAllocator::~ArenaAllocator()
{
    if (m_ownedBuffer)
        m_fallback.free(m_buffer.data());
}

SAllocResult ArenaAllocator::alloc(byte_size bytes, align a)
{
    const byte_size padding = a.padding(m_buffer.data() + m_usedBytes);
    const byte_size correctedSize = a.round_up(bytes);
    const byte_size totalSize = padding + correctedSize;
    const byte_size remaining = m_buffer.size() - m_usedBytes;

    if (totalSize > remaining)
        return m_fallback.alloc(correctedSize, a);

    const byte_size offset = m_usedBytes + padding;
    m_usedBytes += totalSize;

    return SAllocResult {m_buffer.data() + offset, correctedSize};
}

byte_size ArenaAllocator::tryExpand(byte_size bytes, void*) { return 0; }

void ArenaAllocator::free(void* block)
{
    if (!m_buffer.contains(reinterpret_cast<uint8_t*>(block)))
        return m_fallback.free(block);

    // If the block is within the Arena, nothing is done. All memory is freed at the end.
}

} // namespace coll
