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
#include "../src/btree_core.h"
#include "allocator2.h"

#include <cstddef>
#include <stdexcept>

namespace coll
{

template <typename Key, typename Value, byte_size Order = 4>
class bmap
{
    template <typename Key, typename Value, byte_size Order>
    friend class BTreeChecker;

    static void destroyValue(void* valueBuffer) { reinterpret_cast<Value*>(valueBuffer)->~Value(); }

    static void moveValue(void* dest, void* src)
    {
        auto& srcValue = *reinterpret_cast<Value*>(src);
        new (dest) Value(std::move(srcValue));
    }

    static constexpr BTreeCoreParams configure()
    {
        return BTreeCoreParams {Order, sizeof(Value), alignof(Value), destroyValue, moveValue};
    }
    using BTreeCoreType = BTreeCore<Key, configure()>;

public:
    struct InsertResult;
    struct Entry;
    struct Sentinel
    {
    };
    class Handle;
    class Range;
    class InvRange;

    // STL - compatible child types
    using key_type = Key;
    using mapped_type = Value;
    using size_type = BTreeCoreType::size_type;

    bmap(IAllocator& alloc = defaultAllocator())
        : m_core(alloc)
    {
    }

    bmap(std::initializer_list<Entry> init_list, IAllocator& alloc = defaultAllocator());
    ~bmap();

    bmap(const bmap& rhs);
    bmap(bmap&& rhs) noexcept = default;

    bmap& operator=(const bmap& rhs);
    bmap& operator=(bmap&& rhs) noexcept = default;

    InsertResult insert(const Key& key, const Value& value);
    InsertResult insert(const Entry& entry) { return insert(entry.key, entry.value); }

    template <typename... Args>
    InsertResult emplace(const Key& key, Args&&... args);

    template <typename M>
    InsertResult insert_or_assign(const Key& key, M&& obj);

    Handle find(const Key& key) const;
    void clear();

    bool contains(const Key& key) const { return m_core.contains(key); }
    size_type count(const Key& key) const { return m_core.count(key); }

    Range begin() const { return Range(m_core.begin()); }
    Sentinel end() const { return Sentinel(); }

    InvRange rbegin() const { return InvRange(m_core.rbegin()); }
    Sentinel rend() const { return Sentinel(); }

    size_type size() const { return m_core.size(); }
    bool empty() const { return m_core.empty(); }

    Value& operator[](const Key& key);
    const Value& operator[](const Key& key) const { return at(key); }
    const Value& at(const Key& key) const;

    bool erase(const Key& key) { return m_core.erase(key); }

    struct Entry
    {
        const Key& key;
        const Value& value;
    };

    class Handle
    {
    public:
        Handle() = default;

        const Key& key() const { return m_handle.key(); }
        const Value& value() const { return *static_cast<const Value*>(m_handle.value()); }

        bool has_value() const { return m_handle.has_value(); }

        bool operator!=(const Handle& rhs) const = default;
        bool operator==(const Handle& rhs) const = default;

        operator bool() const { return has_value(); }

    private:
        friend class bmap;

        Handle(const BTreeCoreType::Handle& handle)
            : m_handle {handle}
        {
        }

        BTreeCoreType::Handle m_handle;
    }; // class Handle

    struct InsertResult
    {
        Handle location;
        bool inserted;
    };

    class Range
    {
    public:
        Range() = default;

        Entry front() const { return {m_range.key(), *reinterpret_cast<const Value*>(m_range.value())}; }

        const Key& key() const { return m_range.key(); }
        const Value& value() const { return *reinterpret_cast<const Value*>(m_range.value()); }

        bool empty() const { return m_range.empty(); }
        Range begin() const { return *this; }
        Sentinel end() const { return Sentinel(); }

        Entry operator*() const { return front(); }

        bool operator!=(Sentinel) const { return !empty(); }
        bool operator==(Sentinel) const { return empty(); }

        Range& operator++()
        {
            ++m_range;
            return *this;
        }

        Range operator++(int) { return Range(m_range++); }

    private:
        friend class bmap;

        Range(const BTreeCoreType::Range& range)
            : m_range(range)
        {
        }

        BTreeCoreType::Range m_range;
    }; // Class Range

    class InvRange
    {
    public:
        InvRange() = default;

        Entry front() const { return {m_range.key(), *reinterpret_cast<const Value*>(m_range.value())}; }

        const Key& key() const { return m_range.key(); }
        const Value& value() const { return *reinterpret_cast<const Value*>(m_range.value()); }

        bool empty() const { return m_range.empty(); }
        InvRange begin() const { return *this; }
        Sentinel end() const { return Sentinel(); }

        Entry operator*() const { return front(); }

        bool operator!=(Sentinel) const { return !empty(); }
        bool operator==(Sentinel) const { return empty(); }

        InvRange& operator++()
        {
            ++m_range;
            return *this;
        }

        InvRange operator++(int) { return InvRange(m_range++); }

    private:
        friend class bmap;

        InvRange(const BTreeCoreType::InvRange& range)
            : m_range {range}
        {
        }

        BTreeCoreType::InvRange m_range;
    }; // Class Range

private:
    BTreeCoreType m_core;
};

// ------------------------------------------------------------
// Constructores
// ------------------------------------------------------------
template <typename Key, typename Value, byte_size Order>
bmap<Key, Value, Order>::bmap(std::initializer_list<Entry> init_list, IAllocator& alloc)
    : bmap(alloc)
{
    for (const auto& entry : init_list)
        insert(entry);
}

// Definición del constructor de copia
template <typename Key, typename Value, byte_size Order>
bmap<Key, Value, Order>::bmap(const bmap& rhs)
    : bmap(rhs.m_core.allocator())
{
    // Insertamos todos los pares del otro bmap
    for (const auto& entry : rhs)
        insert(entry);
}

// Definición del operador de copia
template <typename Key, typename Value, byte_size Order>
bmap<Key, Value, Order>& bmap<Key, Value, Order>::operator=(const bmap& rhs)
{
    if (this == &rhs)
        return *this;

    clear();
    for (const auto& entry : rhs)
        insert(entry);

    return *this;
}

// ------------------------------------------------------------
// Inicialización y limpieza
// ------------------------------------------------------------

template <typename Key, typename Value, byte_size Order>
bmap<Key, Value, Order>::~bmap()
{
    clear();
}

template <typename Key, typename Value, byte_size Order>
void bmap<Key, Value, Order>::clear()
{
    m_core.clear();
}

// ------------------------------------------------------------
// Inserción pública
// ------------------------------------------------------------
template <typename Key, typename Value, byte_size Order>
typename bmap<Key, Value, Order>::InsertResult
bmap<Key, Value, Order>::insert(const Key& key, const Value& value)
{
    auto [location, valueBuffer, newEntry] = m_core.insert(key);

    // This function does not overwrite previous values.
    if (!newEntry)
        return {Handle(location), false};

    new (valueBuffer) Value(value);
    return {Handle(location), true};
}

template <typename Key, typename Value, byte_size Order>
template <typename... Args>
typename bmap<Key, Value, Order>::InsertResult
bmap<Key, Value, Order>::emplace(const Key& key, Args&&... args)
{
    auto [location, valueBuffer, newEntry] = m_core.insert(key);

    // This function does not overwrite previous values.
    if (!newEntry)
        return {Handle(location), false};

    new (valueBuffer) Value(std::forward<Args>(args)...);
    return {Handle(location), true};
}

template <typename Key, typename Value, byte_size Order>
template <typename M>
typename bmap<Key, Value, Order>::InsertResult
bmap<Key, Value, Order>::insert_or_assign(const Key& key, M&& obj)
{
    auto [location, valueBuffer, newEntry] = m_core.insert(key);

    if (newEntry)
    {
        new (valueBuffer) Value(std::forward<M>(obj));
        return {Handle(location), true};
    }
    else
    {
        Value& prevValue = *reinterpret_cast<Value*>(valueBuffer);
        prevValue = std::forward<M>(obj);
        return {Handle(location), false};
    }
}

// ------------------------------------------------------------
// Búsqueda
// ------------------------------------------------------------
template <typename Key, typename Value, byte_size Order>
typename bmap<Key, Value, Order>::Handle bmap<Key, Value, Order>::find(const Key& key) const
{
    return Handle {m_core.find_first(key)};
}

// ------------------------------------------------------------
// Operator []
// ------------------------------------------------------------

// Definición operator[]
template <typename Key, typename Value, byte_size Order>
Value& bmap<Key, Value, Order>::operator[](const Key& key)
{
    auto [location, valueBuffer, newEntry] = m_core.insert(key);

    if (newEntry)
        return *new (valueBuffer) Value {};
    else
        return *reinterpret_cast<Value*>(valueBuffer);
}

// Devuelve referencia const a Value existente, o lanza si no está.
template <typename Key, typename Value, byte_size Order>
const Value& bmap<Key, Value, Order>::at(const Key& key) const
{
    auto h = find(key);

    if (!h)
        throw std::out_of_range("bmap: key not found");
    else
        return h.value();
}

// ------------------------------------------------------------
// Comparación
// ------------------------------------------------------------
template <typename Key, typename Value, byte_size Order>
std::strong_ordering operator<=>(const bmap<Key, Value, Order>& lhs, const bmap<Key, Value, Order>& rhs)
{
    auto rhs_range = rhs.begin();

    for (const auto& lhs_entry : lhs)
    {
        if (rhs_range.empty())
            return std::strong_ordering::greater;

        const auto& rhs_entry = rhs_range.front();

        if (auto cmp_key = lhs_entry.key <=> rhs_entry.key; cmp_key != 0)
            return cmp_key;

        if (auto cmp_val = lhs_entry.value <=> rhs_entry.value; cmp_val != 0)
            return cmp_val;

        ++rhs_range;
    }

    return rhs_range.empty() ? std::strong_ordering::equal : std::strong_ordering::less;
}

template <typename Key, typename Value, byte_size Order>
bool operator==(const bmap<Key, Value, Order>& lhs, const bmap<Key, Value, Order>& rhs)
{
    return (lhs <=> rhs) == 0;
}
} // namespace coll