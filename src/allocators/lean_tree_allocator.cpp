#include "allocators/lean_tree_allocator.h"

#include <algorithm>
#include <assert.h>
#include <stdexcept>

namespace coll
{
// Example: 0000'0001 << 6 -> 0100'0000 - 1 -> 0011'1111
constexpr uint8_t kLargestFreeBlockMask = (1 << 6) - 1;
constexpr uint8_t kUsedSolid = 0xFF;
constexpr align kHeaderAlign = align::system();

static bool getBit(unsigned* bitData, count_t index)
{
    constexpr size_t wordSize = sizeof(*bitData) * 8;
    const size_t wordOffset = index / wordSize;
    const size_t bitOffset = index % wordSize;
    return (bitData[wordOffset] & (size_t(1) << bitOffset)) != 0;
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

static bool checkFree(uint8_t blockMetadata, uint8_t level)
{
    // TODO: Define it also for lower levels?
    assert(level > 4);
    return (blockMetadata & kLargestFreeBlockMask) >= level;
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

    for (level = 0; level < 5; ++level, curLevelSize /= 2)
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

    if (index < 5)
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

static void initTopLevel(uint8_t* levelData, uint8_t levelIndex, Power2 topLevelSize)
{
    memset(levelData, levelIndex, topLevelSize.value());
}

static uint8_t levelCount(const LeanTreeAllocator::Parameters& params)
{
    return (params.maxAllocSize / params.basicBlockSize).log2();
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
    initTopLevel(m_header->levels[nLevels - 1], nLevels, topLevelSize);

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
        error("Pointer outside managed area");

    const size_t offset = blockPtr - m_header->data;

    // Do not let release the metadata!
    if (offset == 0)
        error("Trying to release meta data");

    // Check that is within the managed area
    if (offset > params.totalSize.value())
        error("Pointer outside managed area");

    // Check basic block alignment
    if (offset % params.basicBlockSize != 0)
        error("Bad alignment");

    const Power2 freed = freeAtBlock(count_t(offset / params.basicBlockSize), topLevel());
    m_stats.bytesUsed -= (freed * params.basicBlockSize).value();
}

SAllocResult LeanTreeAllocator::topLevelAlloc(Power2 basicBlocks)
{
    const count_t nBlocks = (count_t)topLevelBlocksCount().value();
    const uint8_t level = topLevel();
    const uint8_t* levelData = m_header->levels[level];
    uint8_t selectedLfb = kLargestFreeBlockMask;
    count_t selectedIndex = nBlocks;

    for (count_t i = 0; i < nBlocks; ++i)
    {
        const uint8_t lfb = levelData[i] & kLargestFreeBlockMask;

        if (lfb < selectedLfb && lfb >= basicBlocks.log2())
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
        if (level > 4)
        {
            uint8_t* blockMetadata = m_header->levels[level] + index;

            assert(checkFree(*blockMetadata, level));
            *blockMetadata = kUsedSolid;
        }
        else
        {
            setUsedBits(index << level, basicBlocks);
            if (level > 0)
                setSolidBit(level, index);
        }
        return m_header->data + (index << count_t(level));
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
    uint8_t leftLfb;
    uint8_t rightLfb;

    if (level > 5)
    {
        const uint8_t* levelData = m_header->levels[level - 1];
        leftLfb = levelData[index * 2 + 0] & kLargestFreeBlockMask;
        rightLfb = levelData[index * 2 + 1] & kLargestFreeBlockMask;
    }
    else
    {
        leftLfb = lowerLevelsLfb(level - 1, index * 2 + 0);
        rightLfb = lowerLevelsLfb(level - 1, index * 2 + 1);
    }

    if (leftLfb <= rightLfb && leftLfb >= basicBlocks.log2())
        return 0;
    else if (rightLfb < basicBlocks.log2())
    {
        assert(leftLfb >= basicBlocks.log2());
        return 0;
    }
    else
        return 1;
}

bool LeanTreeAllocator::getBitLevelValue(uint8_t level, count_t index) const
{
    assert(level <= 4);

    unsigned* bits = reinterpret_cast<unsigned*>(m_header->levels[level]);

    return getBit(bits, index);
}

void LeanTreeAllocator::setBitLevelValue(uint8_t level, count_t index, bool value)
{
    assert(level <= 4);

    unsigned* bits = reinterpret_cast<unsigned*>(m_header->levels[level]);

    if (value)
        setBits(bits, index, 1);
    else
        clearBits(bits, index, 1);
}

void LeanTreeAllocator::preSplitCheck(uint8_t level, count_t index)
{
    if (level > 4)
    {
        // Niveles 5+: expandir bloque solid a 2 hijos
        uint8_t* parentMetadata = m_header->levels[level] + index;
        assert(*parentMetadata != kUsedSolid);

        const uint8_t parentLfb = *parentMetadata & kLargestFreeBlockMask;

        if (parentLfb == level)
        {
            // The block is solid & free. Split.
            if (level > 5)
            {
                uint8_t* childLevel = m_header->levels[level - 1];
                uint8_t* leftChild = childLevel + (index * 2 + 0);
                uint8_t* rightChild = childLevel + (index * 2 + 1);
                *parentMetadata = *leftChild = *rightChild = uint8_t(level - 1);
            }
            else
            {
                unsigned* level0 = reinterpret_cast<unsigned*>(m_header->levels[0]);

                // Set solid for all children block in all levels.
                count_t bitCount = count_t(1) << (level - 2);
                for (uint8_t i = 1; i < level; ++i)
                {
                    unsigned* childLevel = reinterpret_cast<unsigned*>(m_header->levels[i]);
                    setBits(childLevel, index * bitCount, bitCount);
                    bitCount >>= 1;
                }

                // Set free (all basic blocks)
                clearBits(level0, index << level, size_t(1) << level);
                *parentMetadata = uint8_t(level - 1);
            }
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
    assert(level > 0 && level <= 4);

    unsigned* levelBits = reinterpret_cast<unsigned*>(m_header->levels[level]);
    setBits(levelBits, index, 1);
}

count_t LeanTreeAllocator::lowerLevelsLfb(uint8_t level, count_t index) const
{
    // TODO: This function can be optimized, buts lets live with it by the moment
    if (level == 0)
        return (count_t)getBit(reinterpret_cast<unsigned*>(m_header->levels[0]), index << level);

    const bool solid = getBit(reinterpret_cast<unsigned*>(m_header->levels[level]), index);

    if (solid)
        return level;
    else
    {
        const count_t leftLfb = lowerLevelsLfb(level - 1, index * 2 + 0);
        const count_t rightLfb = lowerLevelsLfb(level - 1, index * 2 + 1);

        return std::max(leftLfb, rightLfb);
    }
}

void LeanTreeAllocator::updateLargestFreeBlock(uint8_t level, count_t index)
{
    if (level <= 4)
        return;

    uint8_t leftLfb, rightLfb;

    if (level == 5)
    {
        leftLfb = lowerLevelsLfb(4, index * 2 + 0);
        rightLfb = lowerLevelsLfb(4, index * 2 + 1);
    }
    else
    {
        const uint8_t* leftChild = m_header->levels[level - 1] + (index * 2 + 0);
        const uint8_t* rightChild = m_header->levels[level - 1] + (index * 2 + 1);
        leftLfb = *leftChild & kLargestFreeBlockMask;
        rightLfb = *rightChild & kLargestFreeBlockMask;
    }

    const uint8_t newLfb = std::max(leftLfb, rightLfb);
    uint8_t* parentMetadata = m_header->levels[level] + index;
    *parentMetadata = newLfb;
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
}

Power2 LeanTreeAllocator::freeAtBlock(count_t basicBlockIndex, uint8_t level)
{
    const count_t blockIndex = basicBlockIndex >> level;

    if (level == 0)
    {
        setBitLevelValue(0, basicBlockIndex, false);
        return Power2::from_log2(level);
    }
    else if (level < 5)
    {
        const bool solid = getBitLevelValue(level, blockIndex);

        if (!solid)
            return freeAtBlock(basicBlockIndex, level - 1);
        else
        {
            if (blockIndex % Power2::from_log2(level) != 0)
                throw std::runtime_error("Pointer does not address the start of an allocated block");

            unsigned* level0 = reinterpret_cast<unsigned*>(m_header->levels[0]);
            setBits(level0, basicBlockIndex, byte_size(1) << level);
            return Power2::from_log2(level);
        }
    }
    else
    {
        uint8_t* levelData = m_header->levels[level];

        if (levelData[blockIndex] == kUsedSolid)
        {
            if (blockIndex % Power2::from_log2(level) != 0)
                throw std::runtime_error("Pointer does not address the start of an allocated block");

            levelData[blockIndex] = level;
            return Power2::from_log2(level);
        }
        else
            return freeAtBlock(basicBlockIndex, level - 1);
    }
}

} // namespace coll
