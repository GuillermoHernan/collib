#include "allocators/lean_tree_allocator.h"

#include <algorithm>
#include <assert.h>
#include <iostream>
#include <stdexcept>

namespace coll
{
constexpr align kHeaderAlign = align::system();
constexpr uint8_t kBitLevelCount = 5;

enum class ByteNode : uint8_t
{
    SolidBit = 0x01,
    UsedBit = 0x02,
    PartialBit = 0x80,
    FreeSolid = 0,
    FullSolid = 0x02,
    FullFragmented = 0x03
};

static bool getBit(unsigned* bitData, count_t index)
{
    constexpr size_t wordSize = sizeof(*bitData) * 8;
    const size_t wordOffset = index / wordSize;
    const size_t bitOffset = index % wordSize;
    return (bitData[wordOffset] & (size_t(1) << bitOffset)) != 0;
}

static unsigned getBits(const unsigned* bitData, count_t index, size_t nBits)
{
    constexpr size_t wordSize = sizeof(*bitData) * 8;
    const size_t wordOffset = index / wordSize;
    const size_t bitOffset = index % wordSize;

    if (nBits == wordSize)
    {
        assert(bitOffset == 0);
        return bitData[wordOffset];
    }
    else
    {
        const unsigned mask = (unsigned(1) << nBits) - 1;
        return (bitData[wordOffset] >> bitOffset) & mask;
    }
}

// Note that 'unsigned' is used on purpose to let the compiler use the most appropiate word size
// for the target architecture.
// Endianness shouldn't be a problem, since The bitfields are always treated a word (unsigned) arrays.
static void setBits(unsigned* bitData, count_t index, size_t nBits)
{
    constexpr size_t wordSize = sizeof(*bitData) * 8;

    const size_t wordOffset = index / wordSize;
    const size_t bitOffset = index % wordSize;
    size_t bitMask = (size_t(1) << nBits) - 1;
    bitMask <<= bitOffset;

    bitData[wordOffset] |= bitMask;
}

static void clearBits(unsigned* bitData, count_t index, size_t nBits)
{
    constexpr size_t wordSize = sizeof(*bitData) * 8;

    const size_t wordOffset = index / wordSize;
    const size_t bitOffset = index % wordSize;
    size_t bitMask = (size_t(1) << nBits) - 1;
    bitMask <<= bitOffset;

    bitData[wordOffset] &= ~bitMask;
}

static LeanTreeAllocator::Parameters validateAndCorrectParams(LeanTreeAllocator::Parameters params)
{
    const uint8_t minLevels = 6;

    params.basicBlockSize = std::max(params.basicBlockSize, Power2::round_up(4));

    const Power2 minTotalSize = params.basicBlockSize << minLevels;
    params.totalSize = std::max(params.totalSize, minTotalSize);
    params.maxAllocSize = std::max(params.maxAllocSize, minTotalSize);
    params.maxAllocSize = std::min(params.maxAllocSize, params.totalSize);
    params.a = std::max(params.a, align::system());

    return params;
}

static Power2 calculateTotalBasicBlocks(const LeanTreeAllocator::Parameters& params)
{
    return params.totalSize / params.basicBlockSize;
}

static byte_size calculateLevelsTotalSize(const Power2 totalBasicBlocks, const uint8_t levels)
{
    const align bitLevelsAlign = align::of<unsigned>();
    const align upperLevelsAlign = align::of<uint8_t>();

    byte_size bitLevelsSize = 0;
    byte_size curLevelSize = totalBasicBlocks.value() / 8;
    uint8_t level;

    for (level = 0; level < kBitLevelCount; ++level, curLevelSize /= 2)
    {
        curLevelSize = std::max(curLevelSize, byte_size(1));
        bitLevelsSize += bitLevelsAlign.round_up(curLevelSize);
    }

    byte_size upperLevelsSize = 0;
    curLevelSize = totalBasicBlocks.value() >> level;
    for (; level < levels; ++level, curLevelSize /= 2)
    {
        curLevelSize = std::max(curLevelSize, byte_size(1));
        upperLevelsSize += upperLevelsAlign.round_up(curLevelSize);
    }

    return bitLevelsSize + upperLevelsSize;
}

static byte_size metaDataSize(
    const LeanTreeAllocator::Parameters& params,
    uint8_t levels,
    Power2 totalBasicBlocks,
    byte_size headerSize
)
{
    byte_size levelsTotalSize = calculateLevelsTotalSize(totalBasicBlocks, levels);
    byte_size levelsTableSize = sizeof(uint8_t*) * levels;

    headerSize = kHeaderAlign.round_up(headerSize);
    levelsTotalSize = kHeaderAlign.round_up(levelsTotalSize);
    levelsTableSize = kHeaderAlign.round_up(levelsTableSize);

    return headerSize + levelsTableSize + levelsTotalSize;
}

static uint8_t* setupLevel(uint8_t* levelPtr, uint8_t index, byte_size totalBasicBlocks)
{
    byte_size size = totalBasicBlocks >> index;

    if (index < kBitLevelCount)
    {
        const align a = align::of<unsigned>();
        size = std::max(a.bytes(), size >> 3);
    }

    return levelPtr + size;
}

static void setupLevels(uint8_t** levels, uint8_t nLevels, Power2 totalBasicBlocks)
{
    uint8_t* nextLevelPtr = reinterpret_cast<uint8_t*>(levels + nLevels);
    kHeaderAlign.apply(nextLevelPtr);

    for (uint8_t i = 0; i < nLevels; ++i)
    {
        levels[i] = nextLevelPtr;
        nextLevelPtr = setupLevel(nextLevelPtr, i, totalBasicBlocks.value());
    }
}

static void initTopLevel(uint8_t* levelData, Power2 topLevelSize)
{
    memset(levelData, uint8_t(ByteNode::FreeSolid), topLevelSize.value());
}

static uint8_t levelCount(const LeanTreeAllocator::Parameters& params)
{
    return (params.maxAllocSize / params.basicBlockSize).log2() + 1;
}

LeanTreeAllocator::LeanTreeAllocator(IAllocator& backing, const Parameters& params_)
    : m_backing(backing)
{
    // 1. Fix allocator parameters
    Parameters params = validateAndCorrectParams(params_);

    // 2. Calculate layout
    const uint8_t nLevels = levelCount(params);
    const Power2 totalBasicBlocks = calculateTotalBasicBlocks(params);
    const byte_size metaSize = metaDataSize(params, nLevels, totalBasicBlocks, sizeof(SHeader));

    // 3. Single memory block allocated
    auto [buffer, _] = m_backing.alloc(params.totalSize.value(), params.a);
    if (buffer == nullptr)
        throw std::bad_alloc();

    // 4. Layout & setup
    uint8_t* rawMemory = reinterpret_cast<uint8_t*>(buffer);
    setupHeader(rawMemory, params);
    setupLevels(m_header->levels, nLevels, totalBasicBlocks);

    // 5. Initial state. Just top level is enough.
    Power2 topLevelSize = topLevelBlocksCount();
    initTopLevel(m_header->levels[nLevels - 1], topLevelSize);

    // 6. Mark metadata allocated.
    allocMetadata(metaSize);

    // 7. Init stats
    m_stats.totalBytes = params.totalSize.value();
}

LeanTreeAllocator::~LeanTreeAllocator() { m_backing.free(m_header->data); }

SAllocResult LeanTreeAllocator::alloc(byte_size bytes, align a)
{
    const Parameters& params = m_header->params;

    Power2 correctedSize = Power2::round_up(bytes);
    correctedSize = std::max(correctedSize, params.basicBlockSize);

    if (correctedSize > params.maxAllocSize)
        return {nullptr, 0};

    const SAllocResult result = topLevelAlloc(correctedSize / params.basicBlockSize);
    if (result.buffer != nullptr)
    {
        AllocLogger::instance().alloc(*this, bytes, correctedSize.value(), result.buffer, a);
        m_stats.bytesUsed += correctedSize.value();
    }

    return result;
}

byte_size LeanTreeAllocator::tryExpand(byte_size bytes, void*)
{
    // TODO: Not implemented, by the moment. In theory, it could try to expand in some cases,
    // but it is somewhat complex.
    // I may try to implement it once the rest is working fine.
    return 0;
}

void LeanTreeAllocator::free(void* buffer)
{
    auto error = [](const char* message) { throw std::runtime_error(message); };

    uint8_t* blockPtr = reinterpret_cast<uint8_t*>(buffer);
    const auto& params = m_header->params;

    if (blockPtr < m_header->data)
        error("LeanTreeAllocator: Pointer outside managed area");

    const size_t offset = blockPtr - m_header->data;

    // Do not let release the metadata!
    if (offset < m_stats.metaDataSize)
        error("LeanTreeAllocator: Trying to release metadata");

    // Check that is within the managed area
    if (offset > params.totalSize.value())
        error("LeanTreeAllocator: Pointer outside managed area");

    // Check basic block alignment
    if (offset % params.basicBlockSize != 0)
        error("LeanTreeAllocator: Bad alignment");

    const Power2 freed = freeAtBlock(count_t(offset / params.basicBlockSize), topLevel());
    m_stats.bytesUsed -= (freed * params.basicBlockSize).value();
}

LeanTreeAllocator::Stats LeanTreeAllocator::stats() const
{
    // Calculate largest free block.
    const count_t nBlocks = (count_t)topLevelBlocksCount().value();
    const uint8_t level = topLevel();
    byte_size selectedLfb = 0;

    for (count_t i = 0; i < nBlocks; ++i)
    {
        const byte_size lfb = byteLevelLfb(0, i);

        if (lfb > selectedLfb)
            selectedLfb = lfb;
    }

    Stats result(m_stats);
    result.largestFreeBlock = selectedLfb;

    return m_stats;
}

bool LeanTreeAllocator::canCoalesce(count_t levelIndex, uint8_t level) const
{
    if (level <= kBitLevelCount)
    {
        const unsigned* level0 = (const unsigned*)m_header->levels[0];
        return 0 == getBits(level0, levelIndex << level, byte_size(1) << level);
    }
    else
    {
        const uint8_t left = m_header->levels[level - 1][levelIndex * 2 + 0];
        const uint8_t right = m_header->levels[level - 1][levelIndex * 2 + 1];

        return left == right && left == uint8_t(ByteNode::FreeSolid);
    }
}

void LeanTreeAllocator::coalesce(count_t levelIndex, uint8_t level)
{
    if (level < kBitLevelCount)
    {
        // Just mark as solid. It is already marked as free at level 0.
        if (canCoalesce(levelIndex, level))
            setBitLevelValue(level, levelIndex, true);
    }
    else
    {
        if (canCoalesce(levelIndex, level))
            m_header->levels[level][levelIndex] = uint8_t(ByteNode::FreeSolid);
        else
            updateLargestFreeBlock(level, levelIndex);
    }
}

void LeanTreeAllocator::dumpSolidBlocks(uint8_t level, count_t index, char separator, std::ostream& csv)
    const
{
    bool isSolid = false;
    bool isUsed = false;

    if (level < kBitLevelCount)
    {
        isSolid = getBitLevelValue(level, index);
        isUsed = getBitLevelValue(0, index << level);
    }
    else
    {
        const uint8_t node = m_header->levels[level][index];
        const uint8_t solidMask = uint8_t(ByteNode::PartialBit) | uint8_t(ByteNode::SolidBit);

        isSolid = (node & solidMask) == 0;
        isUsed = (node & uint8_t(ByteNode::UsedBit)) != 0;
    }

    if (isSolid)
    {
        const Power2 basicSize = m_header->params.basicBlockSize;
        const Power2 blockSize = Power2::from_log2(level);
        const byte_size bytes = (blockSize * basicSize).value();
        const uint8_t* ptr = m_header->data + (index << (level + basicSize.log2()));
        const uintptr_t offset(ptr - m_header->data);
        const char* blockType = "FREE";

        if (isUsed)
            blockType = (offset == 0) ? "META" : "USED";

        csv << blockType;
        csv << separator << "0x" << std::hex << uintptr_t(ptr);
        csv << separator << "0x" << offset;
        csv << separator << std::dec << blockSize.value();
        csv << separator << bytes;
        csv << separator << uint32_t(level);
        csv << std::endl;
    }
    else
    {
        dumpSolidBlocks(level - 1, index * 2 + 0, separator, csv);
        dumpSolidBlocks(level - 1, index * 2 + 1, separator, csv);
    }
}

void LeanTreeAllocator::dumpToCsv(std::ostream& csv, char separator) const
{
    csv << "Used;Pointer;Offset;Basic Blocks;Bytes;Level" << std::endl;

    const uint8_t topLevelIdx = topLevel();
    const count_t blocksInTopLevel = (count_t)topLevelBlocksCount().value();

    for (count_t root = 0; root < blocksInTopLevel; root++)
        dumpSolidBlocks(topLevelIdx, root, separator, csv);
}

SAllocResult LeanTreeAllocator::topLevelAlloc(Power2 basicBlocks)
{
    const count_t nBlocks = (count_t)topLevelBlocksCount().value();
    const uint8_t level = topLevel();
    byte_size selectedLfb = m_header->params.totalSize.value();
    count_t selectedIndex = nBlocks;

    for (count_t i = 0; i < nBlocks; ++i)
    {
        const byte_size lfb = byteLevelLfb(level, i);

        if (lfb < selectedLfb && lfb >= basicBlocks.value())
        {
            selectedIndex = i;
            selectedLfb = lfb;
        }
    }

    if (selectedIndex >= nBlocks)
        return {nullptr, 0};

    // uint8_t* blockStart = m_header->data + (byte_size(selectedIndex) << level);
    uint8_t* allocatedBlock = allocAtLevel(level, selectedIndex, basicBlocks);

    return {allocatedBlock, (basicBlocks * m_header->params.basicBlockSize).value()};
}

uint8_t LeanTreeAllocator::topLevel() const { return levelCount(m_header->params) - 1; }

Power2 LeanTreeAllocator::topLevelBlocksCount() const
{
    const auto& params = m_header->params;
    return params.totalSize / params.maxAllocSize;
}

uint8_t* LeanTreeAllocator::allocAtLevel(uint8_t level, count_t index, Power2 basicBlocks)
{
    // Needs the full block?
    if (basicBlocks.log2() == level)
    {
        if (level >= kBitLevelCount)
        {
            ByteNode* blockMetadata = reinterpret_cast<ByteNode*>(m_header->levels[level] + index);

            assert(*blockMetadata == ByteNode::FreeSolid);
            *blockMetadata = ByteNode::FullSolid;
        }
        else
        {
            setUsedBits(index << level, basicBlocks);
            if (level > 0)
                setSolidBit(level, index);
        }
        return m_header->data + (index << count_t(level + m_header->params.basicBlockSize.log2()));
    }
    else
    {
        preSplitCheck(level, index);
        const count_t childIndex = selectFittingChild(level, index, basicBlocks);
        uint8_t* result = allocAtLevel(level - 1, (index * 2) + childIndex, basicBlocks);
        updateLargestFreeBlock(level, index);
        return result;
    }
}

count_t LeanTreeAllocator::selectFittingChild(uint8_t level, count_t index, Power2 basicBlocks)
{
    byte_size leftLfb;
    byte_size rightLfb;

    if (level > kBitLevelCount)
    {
        leftLfb = byteLevelLfb(level - 1, index * 2 + 0);
        rightLfb = byteLevelLfb(level - 1, index * 2 + 1);
    }
    else
    {
        leftLfb = lowerLevelLfb(level - 1, index * 2 + 0);
        rightLfb = lowerLevelLfb(level - 1, index * 2 + 1);
    }

    const byte_size requestedSize = basicBlocks.value();

    if (leftLfb <= rightLfb && leftLfb >= requestedSize)
        return 0;
    else if (rightLfb < requestedSize)
    {
        assert(leftLfb >= requestedSize);
        return 0;
    }
    else
        return 1;
}

bool LeanTreeAllocator::getBitLevelValue(uint8_t level, count_t index) const
{
    assert(level < kBitLevelCount);

    unsigned* bits = reinterpret_cast<unsigned*>(m_header->levels[level]);

    return getBit(bits, index);
}

void LeanTreeAllocator::setBitLevelValue(uint8_t level, count_t index, bool value)
{
    assert(level < kBitLevelCount);

    unsigned* bits = reinterpret_cast<unsigned*>(m_header->levels[level]);

    if (value)
        setBits(bits, index, 1);
    else
        clearBits(bits, index, 1);
}

void LeanTreeAllocator::preSplitCheck(uint8_t level, count_t index)
{
    if (level < kBitLevelCount)
    {
        // Clear solid bit. Note that for level zero it will set the block as 'free'. But it should
        // already be free.
        setBitLevelValue(level, index, false);
    }
    else
    {
        uint8_t* parentMetadata = m_header->levels[level] + index;
        assert(*parentMetadata != uint8_t(ByteNode::FullSolid));

        if (*parentMetadata == uint8_t(ByteNode::FreeSolid))
        {
            // The block is solid & free. Split.
            if (level > kBitLevelCount)
            {
                uint8_t* childLevel = m_header->levels[level - 1];
                uint8_t* leftChild = childLevel + (index * 2 + 0);
                uint8_t* rightChild = childLevel + (index * 2 + 1);

                *leftChild = *rightChild = uint8_t(ByteNode::FreeSolid);
            }
            else
            {
                // Set free (all basic blocks)
                unsigned* level0 = reinterpret_cast<unsigned*>(m_header->levels[0]);
                count_t bitCount = count_t(1) << level;
                index <<= level;
                clearBits(level0, index, bitCount);

                // Set solid for all intermediate 1 bit blocks
                for (uint8_t i = 1; i < level; ++i)
                {
                    index >>= 1;
                    bitCount >>= 1;
                    unsigned* childLevel = reinterpret_cast<unsigned*>(m_header->levels[i]);
                    setBits(childLevel, index, bitCount);
                }
            }

            *parentMetadata = uint8_t(ByteNode::PartialBit) + (level - 1);
        }
    }
}

void LeanTreeAllocator::setUsedBits(count_t index, Power2 size)
{
    // Level zero contains the used / free information of individual basic blocks. 1 bit per block
    unsigned* level0 = reinterpret_cast<unsigned*>(m_header->levels[0]);
    setBits(level0, index, size.value());
}

void LeanTreeAllocator::setSolidBit(uint8_t level, count_t index)
{
    assert(level > 0 && level < kBitLevelCount);

    unsigned* levelBits = reinterpret_cast<unsigned*>(m_header->levels[level]);
    setBits(levelBits, index, 1);
}

byte_size LeanTreeAllocator::lowerLevelLfb(uint8_t level, count_t index) const
{
    // TODO: Possible optimization -> Use a table for the first levels.
    // Up to level 2 would require 16 entries. But 256 entries for level 3 (this one is perhaps too
    // much).
    if (level == 0)
    {
        const bool used = getBit(reinterpret_cast<unsigned*>(m_header->levels[0]), index);
        return used ? 0 : 1;
    }

    const bool solid = getBit(reinterpret_cast<unsigned*>(m_header->levels[level]), index);

    if (solid)
    {
        const bool used = getBit(reinterpret_cast<unsigned*>(m_header->levels[0]), index << level);
        return used ? 0 : (byte_size(1) << level);
    }
    else
    {
        const byte_size leftLfb = lowerLevelLfb(level - 1, index * 2 + 0);
        const byte_size rightLfb = lowerLevelLfb(level - 1, index * 2 + 1);

        return std::max(leftLfb, rightLfb);
    }
}

byte_size LeanTreeAllocator::byteLevelLfb(uint8_t level, count_t index) const
{
    const uint8_t nodeData = m_header->levels[level][index];

    if (nodeData & uint8_t(ByteNode::PartialBit))
    {
        const uint8_t logValue = nodeData & ~uint8_t(ByteNode::PartialBit);
        return byte_size(1) << logValue;
    }
    else if (nodeData == uint8_t(ByteNode::FreeSolid))
        return byte_size(1) << level;
    else
        return 0;
}

void LeanTreeAllocator::updateLargestFreeBlock(uint8_t level, count_t index)
{
    if (level < kBitLevelCount)
        return;

    uint8_t* parentMetadata = m_header->levels[level] + index;
    byte_size leftLfb, rightLfb;

    if (level == kBitLevelCount)
    {
        leftLfb = lowerLevelLfb(level - 1, index * 2 + 0);
        rightLfb = lowerLevelLfb(level - 1, index * 2 + 1);
    }
    else
    {
        leftLfb = byteLevelLfb(level - 1, index * 2 + 0);
        rightLfb = byteLevelLfb(level - 1, index * 2 + 1);
    }

    if (leftLfb + rightLfb == 0)
    {
        *parentMetadata = uint8_t(ByteNode::FullFragmented);
        return;
    }
    else
    {
        const Power2 newLfb = Power2::round_down(std::max(leftLfb, rightLfb));
        *parentMetadata = uint8_t(ByteNode::PartialBit) | newLfb.log2();
    }
}

void LeanTreeAllocator::setupHeader(uint8_t* rawMemory, const Parameters& params)
{
    m_header = new (rawMemory) SHeader();

    m_header->data = rawMemory;
    m_header->levels = reinterpret_cast<uint8_t**>(m_header + 1);
    m_header->params = params;
}

void LeanTreeAllocator::allocMetadata(byte_size size)
{
    const auto& params = m_header->params;
    const Power2 correctedSize = Power2::round_up(size);
    const uint8_t nLevels = levelCount(params);

    const Power2 bytes = std::max(correctedSize, params.basicBlockSize);
    const Power2 basicBlocks = bytes / params.basicBlockSize;

    uint8_t* buffer = allocAtLevel(nLevels - 1, 0, basicBlocks);
    assert(buffer == m_header->data);
    m_stats.metaDataSize = correctedSize.value();
}

Power2 LeanTreeAllocator::freeAtBlock(count_t basicBlockIndex, uint8_t level)
{
    const count_t blockIndex = basicBlockIndex >> level;

    if (level == 0)
    {
        setBitLevelValue(0, basicBlockIndex, false);
        return Power2::from_log2(level);
    }
    else if (level < kBitLevelCount)
    {
        const bool solid = getBitLevelValue(level, blockIndex);

        if (!solid)
        {
            const Power2 result = freeAtBlock(basicBlockIndex, level - 1);
            coalesce(basicBlockIndex >> level, level);
            return result;
        }
        else
        {
            if (basicBlockIndex % Power2::from_log2(level) != 0)
                throw std::runtime_error(
                    "LeanTreeAllocator: Pointer does not address the start of an allocated block"
                );

            unsigned* level0 = reinterpret_cast<unsigned*>(m_header->levels[0]);
            clearBits(level0, basicBlockIndex, byte_size(1) << level);
            return Power2::from_log2(level);
        }
    }
    else
    {
        uint8_t* levelData = m_header->levels[level];

        if (levelData[blockIndex] == uint8_t(ByteNode::FullSolid))
        {
            if (basicBlockIndex % Power2::from_log2(level) != 0)
                throw std::runtime_error(
                    "LeanTreeAllocator: Pointer does not address the start of an allocated block"
                );

            levelData[blockIndex] = uint8_t(ByteNode::FreeSolid);
            return Power2::from_log2(level);
        }
        else
        {
            const Power2 result = freeAtBlock(basicBlockIndex, level - 1);
            coalesce(basicBlockIndex >> level, level);
            return result;
        }
    }
}

bool LeanTreeAllocator::validate(std::ostream& log) const
{
    bool allOk = true;

    if (!m_header || !m_header->data || !m_header->levels)
    {
        log << "[ERROR] Null header, data or levels pointer\n";
        return false;
    }

    const uint8_t nLevels = levelCount(m_header->params);
    const uint8_t topLevelIdx = topLevel();
    const count_t topBlocksCount = (count_t)topLevelBlocksCount().value();

    if (topLevelIdx + 1 != nLevels)
    {
        log << "[ERROR] Top level (" << int(topLevelIdx) << ") doesn't match levels count ("
            << int(nLevels) << ")\n";
        allOk = false;
    }

    // Validate blocks, recursively.
    for (count_t root = 0; root < topBlocksCount; ++root)
        allOk &= validateLevel(topLevelIdx, root, log);

    allOk &= validateStats(log);

    return allOk;
}

static std::ostream& reportBlockLocation(uint8_t level, count_t index, std::ostream& log)
{
    log << " level=" << unsigned(level) << " index=" << index;
    return log;
}

bool LeanTreeAllocator::validateLevel(uint8_t level, count_t index, std::ostream& log) const
{
    if (level == 0)
        return true;

    if (level < kBitLevelCount)
    {
        bool ok = true;

        const bool isSolid = getBitLevelValue(level, index);

        if (isSolid)
            ok &= validateSolidBlock(level, index, log);

        // Always check children. If bit levels are active, they are active from top to bottom.
        ok &= validateLevel(level - 1, index * 2 + 0, log);
        ok &= validateLevel(level - 1, index * 2 + 1, log);

        return ok;
    }
    else
    {
        // validateByteNode will recursively call this function (if required) to validate its children.
        return validateByteNode(level, index, log);
    }
}

bool LeanTreeAllocator::validateSolidBlock(uint8_t level, count_t index, std::ostream& log) const
{
    // A solid block must have all its basic blocks in the same state at level 0
    const count_t baseIndex = index << level;
    const count_t blockSize = count_t(1) << level;
    const unsigned mask = (unsigned(1) << blockSize) - 1;
    const unsigned* level0 = (const unsigned*)m_header->levels[0];
    const unsigned basicBlockBits = getBits(level0, baseIndex, blockSize);
    const bool firstUsed = getBitLevelValue(0, baseIndex);

    if (basicBlockBits == 0 || basicBlockBits == mask)
        return true;

    log << "[ERROR] Solid block at";
    reportBlockLocation(level, index, log);
    log << " has inconsistent level-0 bits: 0x" << std::hex << basicBlockBits << std::dec << "\n";

    return false;
}

bool LeanTreeAllocator::validateByteNode(uint8_t level, count_t index, std::ostream& log) const
{
    const uint8_t nodeValue = m_header->levels[level][index];
    const uint8_t partialMask = uint8_t(ByteNode::PartialBit);
    const uint8_t solidMask = uint8_t(ByteNode::PartialBit) | uint8_t(ByteNode::SolidBit);

    // Solid blocks do not require further check
    if (nodeValue == uint8_t(ByteNode::FreeSolid) || nodeValue == uint8_t(ByteNode::FullSolid))
        return true;

    byte_size leftLfb, rightLfb;

    if (level == kBitLevelCount)
    {
        leftLfb = lowerLevelLfb(level - 1, index * 2 + 0);
        rightLfb = lowerLevelLfb(level - 1, index * 2 + 1);
    }
    else
    {
        leftLfb = byteLevelLfb(level - 1, index * 2 + 0);
        rightLfb = byteLevelLfb(level - 1, index * 2 + 1);
    }

    bool ok = true;

    if (nodeValue == uint8_t(ByteNode::FullFragmented))
    {
        if (leftLfb != 0 || rightLfb != 0)
        {
            log << "[ERROR] Invalid ByteNode at";
            reportBlockLocation(level, index, log)
                << ". It is marked as full, but its children have some free space (" << leftLfb << ", "
                << rightLfb << ")\n";
            ok = false;
        }
    }
    else if ((nodeValue & partialMask) != 0)
    {
        // Partial nodes: PartialBit + log2 value
        const Power2 nodeLfb = Power2::from_log2(nodeValue & ~partialMask);

        if (nodeLfb.value() != std::max(leftLfb, rightLfb))
        {
            log << "[ERROR] Invalid ByteNode at";
            reportBlockLocation(level, index, log);
            log << ". Its largest free block value (" << nodeLfb.value()
                << ") does not match its children (" << leftLfb << ", " << rightLfb << ")\n";

            ok = false;
        }
        else if (leftLfb == rightLfb && leftLfb == byte_size(1) << (level - 1))
        {
            log << "[ERROR] Invalid ByteNode at";
            reportBlockLocation(level, index, log);
            log << ". Both children are fully free and the node is marked as 'partial'\n";

            ok = false;
        }
    }
    else
    {
        log << "[ERROR] Unexpected ByteNode value (0x" << std::hex << unsigned(nodeValue) << std::dec << ") at";
        reportBlockLocation(level, index, log) << "\n";
        ok = false;
    }

    // Validate children for non-solid nodes.
    ok &= validateLevel(level - 1, index * 2 + 0, log);
    ok &= validateLevel(level - 1, index * 2 + 1, log);

    return ok;
}

bool LeanTreeAllocator::validateStats(std::ostream& log) const
{
    const uint8_t topLevelIdx = topLevel();
    const count_t topBlocksCount = (count_t)topLevelBlocksCount().value();
    count_t usedBlocksCount = 0;

    for (count_t i = 0; i < topBlocksCount; ++i)
        usedBlocksCount += countUsedBlocks(topLevelIdx, i);

    const byte_size usedBytes = usedBlocksCount * m_header->params.basicBlockSize.value();

    if (usedBytes != m_stats.bytesUsed + m_stats.metaDataSize)
    {
        log << "[ERROR]: Used bytes count mismatch. Metadata (" << m_stats.metaDataSize
            << ") + bytesUsed (" << m_stats.bytesUsed << ") != RealUsedCount (" << usedBytes << ")\n";
        return false;
    }

    return true;
}

count_t LeanTreeAllocator::countUsedBlocks(uint8_t level, count_t levelIndex) const
{
    if (level < kBitLevelCount)
    {
        // At lower (bit) levels, just count bits
        const unsigned bitCount = 1 << level;
        const unsigned* level0 = (const unsigned*)m_header->levels[0];
        unsigned bits = getBits(level0, levelIndex << level, bitCount);
        count_t usedCount = 0;

        for (; bits != 0; bits >>= 1)
            usedCount += bits & 1;

        return usedCount;
    }
    else
    {
        const uint8_t nodeValue = m_header->levels[level][levelIndex];

        if (0 == (nodeValue & uint8_t(ByteNode::PartialBit)))
        {
            if (nodeValue & uint8_t(ByteNode::UsedBit))
                return count_t(1) << level;
            else
                return 0;
        }
        else
        {
            // Check children for partially used nodes;
            return countUsedBlocks(level - 1, levelIndex * 2 + 0)
                + countUsedBlocks(level - 1, levelIndex * 2 + 1);
        }
    }
}

} // namespace coll
