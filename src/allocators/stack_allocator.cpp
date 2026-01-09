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

#include "allocators/stack_allocator.h"

#include <assert.h>
#include <iostream>
#include <stdexcept>

namespace coll
{

// ChunkData stores:
//  - offset: 28 bits
//  - aligment: 3 bits
//  - used flag: 1 bit.
class ChunkData
{
public:
    // Marks the chunk as used, initially.
    ChunkData(uint32_t offset, align a)
        : m_data((offset << 4) | 1)
    {
        const uint32_t alignValue = a.log2Size() - StackAllocator::Limits::minAlign.log2Size();
        m_data |= alignValue << 1;
    }

    uint32_t offset() const { return m_data >> 4; }
    bool used() const { return (m_data & 1) != 0; }
    align align()
    {
        const uint32_t alignValue = ((m_data >> 1) & 0x7) + StackAllocator::Limits::minAlign.log2Size();
        return align::from_log2(alignValue);
    }

    void free() { m_data &= ~1; }
    void add(uint32_t offset) { m_data += (offset << 4); }

private:
    uint32_t m_data;
};

struct SBlockHeader
{
    SBlockHeader* next = nullptr;
    count_t allocCount = 0;
    count_t capacity;
    count_t dataBytesUsed = 0;

    void* data() const { return (void*)(this + 1); }

    void* freeSpace() const
    {
        uint8_t* base = reinterpret_cast<uint8_t*>(data());

        return base + dataBytesUsed;
    }

    count_t freeBytes() const
    {
        // clang-format off
        return capacity
            - dataBytesUsed
            - count_t(allocCount * sizeof(ChunkData));
        // clang-format on
    }

    count_t maxChunkSize() const
    {
        const count_t free = freeBytes();

        if (free >= sizeof(ChunkData))
            return free - sizeof(ChunkData);
        else
            return 0;
    }

    ChunkData* chunksTop() { return chunksEnd() - allocCount; }

    const ChunkData* chunksTop() const { return chunksEnd() - allocCount; }

    ChunkData* chunksEnd()
    {
        uint8_t* base = reinterpret_cast<uint8_t*>(data());
        return reinterpret_cast<ChunkData*>(base + capacity);
    }

    const ChunkData* chunksEnd() const
    {
        uint8_t* base = reinterpret_cast<uint8_t*>(data());
        return reinterpret_cast<ChunkData*>(base + capacity);
    }

    SAllocResult pushChunk(count_t bytes, align a)
    {
        uint8_t* prevFreeSpace = (uint8_t*)freeSpace();
        const count_t padding = (count_t)a.padding(prevFreeSpace);

        ChunkData* lastChunk = chunksTop();

        --lastChunk;
        ++this->allocCount;

        new (lastChunk) ChunkData(dataBytesUsed + padding, a);
        dataBytesUsed += bytes + padding;

        return SAllocResult {prevFreeSpace + padding, bytes};
    }
};

struct StackAllocator::BlockHeader : public SBlockHeader
{
};

static bool fitsInBlock(const SBlockHeader* block, byte_size correctedSize, align a)
{
    if (block == nullptr)
        return false;

    void* freeSpace = block->freeSpace();
    const byte_size padding = a.padding(freeSpace);
    const byte_size totalSize = padding + correctedSize;

    return block->maxChunkSize() >= totalSize;
}

static bool tryFree(SBlockHeader& block, uint8_t* buffer)
{
    // Shouldn't try to free in empty blocks.
    assert(block.allocCount > 0);

    uint8_t* base = reinterpret_cast<uint8_t*>(block.data());
    uint8_t* blockEnd = base + block.capacity;

    // Check if in range.
    if (buffer < base || buffer >= blockEnd)
        return false;

    ChunkData* chunksEnd = block.chunksEnd();
    ChunkData* firstChunk = chunksEnd - block.allocCount;

    // Look for the chunk, starting for the last (chunks info are in reverse order)
    for (ChunkData* chunk = firstChunk; chunk < chunksEnd; ++chunk)
    {
        uint8_t* chunkStart = base + chunk->offset();
        if (buffer == chunkStart)
        {
            chunk->free();
            return true;
        }
    }

    return false;
}

StackAllocator::StackAllocator(IAllocator& backing, const Parameters& params)
    : m_backing(backing)
    , m_params(params)
{
    m_params.minBlockSize = std::max(params.minBlockSize, Limits::minBlockSize);
    m_params.maxBlockSize = std::max(params.minBlockSize, params.maxBlockSize);
    m_params.maxBlockSize = std::min(params.maxBlockSize, Limits::maxBlockSize);
}

StackAllocator::~StackAllocator()
{
    while (m_firstBlock != nullptr)
    {
        BlockHeader* next = static_cast<BlockHeader*>(m_firstBlock->next);
        m_backing.free(m_firstBlock);
        m_firstBlock = next;
    }
}

SAllocResult StackAllocator::alloc(byte_size bytes, align a)
{
    if (a > Limits::maxAlign)
        return {nullptr, 0};

    if (bytes > Limits::maxAllocSize)
        return {nullptr, 0};

    a = std::max(a, Limits::minAlign);
    const byte_size correctedSize = a.round_up(bytes);

    if (!fitsInBlock(m_firstBlock, correctedSize, a))
        pushNewBlock(correctedSize, a);

    ++m_stats.allocCount;
    SAllocResult result = m_firstBlock->pushChunk(count_t(correctedSize), a);

    AllocLogger::instance().alloc(*this, bytes, result.bytes, result.buffer, a);
    return result;
}

byte_size StackAllocator::tryExpand(byte_size bytes, void* ptr)
{
    uint8_t* buffer = reinterpret_cast<uint8_t*>(ptr);
    SBlockHeader* curBlock = m_firstBlock;

    auto logResult = [bytes, ptr, this](byte_size result)
    {
        AllocLogger::instance().tryExpand(*this, bytes, result, ptr);
        return result;
    };

    for (; curBlock != nullptr; curBlock = curBlock->next)
    {
        uint8_t* base = reinterpret_cast<uint8_t*>(curBlock->data());
        uint8_t* blockEnd = base + curBlock->capacity;

        if (buffer < base || buffer >= blockEnd)
            continue;

        ChunkData* topChunk = curBlock->chunksTop();
        if (buffer != base + topChunk->offset())
            return logResult(0);

        const byte_size available = curBlock->freeBytes();
        const align a = topChunk->align();
        byte_size currentSize = curBlock->dataBytesUsed - topChunk->offset();
        byte_size maxChunkSize = available + currentSize;
        maxChunkSize = a.round_down(maxChunkSize);
        const byte_size newSize = std::min(maxChunkSize, std::max(bytes, currentSize));

        if (newSize > currentSize)
        {
            curBlock->dataBytesUsed += count_t(newSize - currentSize);
            return logResult(newSize);
        }
        else
            return logResult(0);
    }

    return logResult(0);
}

void StackAllocator::free(void* buffer)
{
    SBlockHeader* curBlock = m_firstBlock;
    auto* bufferBytes = reinterpret_cast<uint8_t*>(buffer);

    for (; curBlock != nullptr; curBlock = curBlock->next)
    {
        if (tryFree(*curBlock, bufferBytes))
        {
            --m_stats.allocCount;
            cleanAfterFree();
            AllocLogger::instance().free(*this, buffer);
            return;
        }
    }

    throw std::runtime_error("STackAllocator: Error freeing chunk. Not found in Stack Allocator");
}

void StackAllocator::cleanAfterFree()
{
    while (m_firstBlock != nullptr)
    {
        // Release memory from free chunks.
        while (m_firstBlock->allocCount > 0)
        {
            ChunkData* topChunk = m_firstBlock->chunksTop();
            if (topChunk->used())
                break;

            m_firstBlock->dataBytesUsed = topChunk->offset();
            --m_firstBlock->allocCount;
        }

        // It will free the block and proceed to the next one only if the current block is empty.
        if (m_firstBlock->allocCount > 0)
            break;

        SBlockHeader* nextBlock = m_firstBlock->next;

        m_stats.totalMemory -= m_firstBlock->capacity + sizeof(SBlockHeader);
        --m_stats.blockCount;

        m_backing.free(m_firstBlock);
        m_firstBlock = static_cast<BlockHeader*>(nextBlock);
    }
}

void StackAllocator::pushNewBlock(byte_size allocSize, align a)
{
    // Update allocSize to take Block overhead into account.
    allocSize += a.round_up(sizeof(SBlockHeader));
    allocSize += a.round_up(sizeof(ChunkData));

    // Calculate new size. Note that a big alloc size can create a block bigger than 'maxBlockSize'
    byte_size blockSize = std::max(byte_size(m_params.minBlockSize), m_stats.totalMemory);
    blockSize = std::min(blockSize, byte_size(m_params.maxBlockSize));
    blockSize = std::max(byte_size(m_params.minBlockSize), allocSize);

    // Use backing allocator to allocate the new block.
    auto [rawBlock, allocatedSize] = m_backing.alloc(blockSize, align::of<SBlockHeader>());
    BlockHeader* newBlock = new (rawBlock) BlockHeader();

    // Fill header
    newBlock->next = m_firstBlock;
    newBlock->capacity = count_t(allocatedSize - sizeof(SBlockHeader));
    newBlock->dataBytesUsed = 0;
    newBlock->allocCount = 0;

    m_firstBlock = newBlock;

    // Update stats
    m_stats.totalMemory += allocatedSize;
    ++m_stats.blockCount;
}

static bool validateBlock(const SBlockHeader& block)
{
    if (block.capacity == 0)
        return false;

    if (block.dataBytesUsed > block.capacity)
        return false;

    const count_t maxChunkCount = block.capacity / sizeof(ChunkData);
    if (block.allocCount > maxChunkCount)
        return false;

    return true;
}

static bool validateChunk(const SBlockHeader& block, const ChunkData* chunk)
{
    uint8_t* base = reinterpret_cast<uint8_t*>(block.data());
    uint32_t offset = chunk->offset();

    // Check that the offset is within the block.
    if (offset >= block.capacity)
        return false;

    // Offset must be within used area.
    if (offset > block.dataBytesUsed)
        return false;

    // The offset is greater or equal than next chunk offset. Zero size chunks are allowed.
    const ChunkData* next = chunk + 1;

    if (next < block.chunksEnd() && next->offset() > chunk->offset())
        return false;

    return true;
}

static void chunkToCsv(
    std::ostream& csv,
    int blockIndex,
    count_t offset,
    count_t size,
    void* address,
    std::string_view status,
    char separator
)
{
    csv << blockIndex << separator << offset;

    if (address == nullptr)
        csv << separator << "BAD" << separator << "BAD";
    else
        csv << separator << size << separator << std::hex << std::uppercase << address << std::dec;

    csv << separator << status;
    csv << "\n";
}

static void writeBadChunks(std::ostream& csv, int blockIndex, size_t remainingChunks, char separator)
{
    for (size_t i = 0; i < remainingChunks; ++i)
    {
        chunkToCsv(csv, blockIndex, 0, 0, nullptr, "UNKNOWN", separator);
    }
}

void StackAllocator::dumpToCsv(std::ostream& csv, char separator) const
{
    // Header
    // clang-format off
    csv << "Block" 
        << separator << "Offset" 
        << separator << "Size" 
        << separator << "Address" 
        << separator << "Status\n";
    // clang-format on

    SBlockHeader* block = m_firstBlock;
    int blockIndex = 0;

    while (block != nullptr)
    {
        uint8_t* base = reinterpret_cast<uint8_t*>(block->data());
        ChunkData* chunksEnd = block->chunksEnd();
        count_t chunkEndOffset = block->dataBytesUsed;

        for (ChunkData* chunk = block->chunksTop(); chunk < chunksEnd; ++chunk)
        {
            std::string_view status = chunk->used() ? "USED" : "FREE";

            if (!validateChunk(*block, chunk))
            {
                chunkToCsv(csv, blockIndex, chunk->offset(), 0, nullptr, status, separator);
                writeBadChunks(csv, blockIndex, static_cast<size_t>(chunksEnd - chunk - 1), separator);
                break;
            }

            uint8_t* chunkStart = base + chunk->offset();
            const count_t chunkSize = chunkEndOffset - chunk->offset();
            chunkEndOffset = chunk->offset();

            chunkToCsv(csv, blockIndex, chunk->offset(), chunkSize, chunkStart, status, separator);
        }

        block = block->next;
        ++blockIndex;
    }
}

bool StackAllocator::validate() const
{
    if (m_firstBlock == nullptr)
    {
        return m_stats.allocCount == 0 && m_stats.blockCount == 0 && m_stats.totalMemory == 0;
    }

    for (const SBlockHeader* block = m_firstBlock; block != nullptr; block = block->next)
    {
        if (!validateBlock(*block))
            return false;

        // Validate chunks within block.
        const ChunkData* chunksEnd = block->chunksEnd();

        for (const ChunkData* chunk = block->chunksTop(); chunk < chunksEnd; ++chunk)
        {
            if (!validateChunk(*block, chunk))
                return false;
        }
    }

    return true;
}

} // namespace coll
