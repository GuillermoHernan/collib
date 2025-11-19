#pragma once
#include "allocator2.h"
#include "btree_core.h"

#include <cstddef>

template <typename Key, typename Value, size_t Order = 4>
class BTreeMap
{
    template <typename Key, typename Value, size_t Order>
    friend class BTreeChecker;

    static void destroyValue(void* valueBuffer)
    {
        reinterpret_cast<Value*>(valueBuffer)->~Value();
    }

    static void moveValue(void* dest, void* src)
    {
        auto& srcValue = *reinterpret_cast<Value*>(src);
        new (dest) Value(std::move(srcValue));
    }

    static constexpr BTreeCoreParams configure()
    {
        return BTreeCoreParams{ Order, sizeof(Value), alignof(Value), destroyValue, moveValue };
    }
    using BTreeCoreType = BTreeCore<Key, configure()>;

public:
    struct InsertResult;
    struct Entry;
    struct Sentinel {};
    class Handle;
    class Range;
    class InvRange;

    // STL - compatible child types
    using key_type = Key;
    using mapped_type = Value;

    BTreeMap(IAllocator& alloc = defaultAllocator()) : m_core(alloc) {}
    BTreeMap(
        std::initializer_list<Entry> init_list
        , IAllocator& alloc = defaultAllocator()
    );
    ~BTreeMap();

    BTreeMap(const BTreeMap& rhs);
    BTreeMap(BTreeMap&& rhs) noexcept = default;

    BTreeMap& operator=(const BTreeMap& rhs);
    BTreeMap& operator=(BTreeMap&& rhs) noexcept = default;

    InsertResult insert(const Key& key, const Value& value);
    InsertResult insert(const Entry& entry) { return insert(entry.key, entry.value); }

    template <typename... Args>
    InsertResult emplace(const Key& key, Args&&... args);

    template <typename M>
    InsertResult insert_or_assign(const Key& key, M&& obj);

    Handle find(const Key& key) const;
    void clear();

    bool contains(const Key& key) const { return m_core.contains(key); }
    size_t count(const Key& key) const { return m_core.count(key); }

    Range begin() const { return Range(m_core.begin()); }
    Sentinel end() const { return Sentinel(); }

    InvRange rbegin() const { return InvRange(m_core.rbegin()); }
    Sentinel rend() const { return Sentinel(); }

    size_t size() const { return m_core.size();}
    bool empty() const { return m_core.empty();}

    Value& operator[](const Key& key);
    const Value& operator[](const Key& key)const { return at(key); }
    const Value& at(const Key& key)const;

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

        const Key& key()const { return m_handle.key(); }
        const Value& value()const { return *static_cast<const Value*>(m_handle.value()); }

        bool has_value() const { return m_handle.has_value();  }

        bool operator!=(const Handle& rhs) const = default;
        bool operator ==(const Handle& rhs) const = default;

        operator bool()const { return has_value(); }

    private:
        friend class BTreeMap;

        Handle(const BTreeCoreType::Handle& handle) : m_handle{ handle } {}

        BTreeCoreType::Handle m_handle;
    };// class Handle

    struct InsertResult
    {
        Handle location;
        bool inserted;
    };

    class Range
    {
    public:
        Range() = default;

        Entry front() const
        {
            return { m_range.key(), *reinterpret_cast<const Value*>(m_range.value()) };
        }

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

        Range operator++(int)
        {
            return Range(m_range++);
        }

    private:
        friend class BTreeMap;

        Range(const BTreeCoreType::Range& range) : m_range(range) {}

        BTreeCoreType::Range m_range;
    }; // Class Range

    class InvRange
    {
    public:
        InvRange() = default;

        Entry front() const
        {
            return { m_range.key(), *reinterpret_cast<const Value*>(m_range.value()) };
        }

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

        InvRange operator++(int)
        {
            return InvRange(m_range++);
        }

    private:
        friend class BTreeMap;

        InvRange(const BTreeCoreType::InvRange& range) : m_range{ range } {}

        BTreeCoreType::InvRange m_range;
    }; // Class Range

private:
    BTreeCoreType m_core;
};

// ------------------------------------------------------------
// Constructores
// ------------------------------------------------------------
template <typename Key, typename Value, size_t Order>
BTreeMap<Key, Value, Order>::BTreeMap(std::initializer_list<Entry> init_list, IAllocator& alloc)
    : BTreeMap(alloc)
{
    for (const auto& entry : init_list)
        insert(entry);
}

// Definición del constructor de copia
template <typename Key, typename Value, size_t Order>
BTreeMap<Key, Value, Order>::BTreeMap(const BTreeMap& rhs)
    :BTreeMap(rhs.m_core.allocator())
{
    // Insertamos todos los pares del otro BTreeMap
    for (const auto &entry: rhs)
        insert(entry);
}

// Definición del operador de copia
template <typename Key, typename Value, size_t Order>
BTreeMap<Key, Value, Order>& BTreeMap<Key, Value, Order>::operator=(const BTreeMap& rhs)
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

template <typename Key, typename Value, size_t Order>
BTreeMap<Key, Value, Order>::~BTreeMap()
{
    clear();
}

template <typename Key, typename Value, size_t Order>
void BTreeMap<Key, Value, Order>::clear()
{
    m_core.clear();
}

// ------------------------------------------------------------
// Inserción pública
// ------------------------------------------------------------
template <typename Key, typename Value, size_t Order>
typename BTreeMap<Key, Value, Order>::InsertResult
BTreeMap<Key, Value, Order>::insert(const Key& key, const Value& value)
{
    auto [location, valueBuffer, newEntry] = m_core.insert(key);

    // This function does not overwrite previous values.
    if (!newEntry)
        return { Handle(location), false };

    new (valueBuffer) Value(value);
    return { Handle(location), true };
}

template <typename Key, typename Value, size_t Order>
template <typename... Args>
typename BTreeMap<Key, Value, Order>::InsertResult
BTreeMap<Key, Value, Order>::emplace(const Key& key, Args&&... args)
{
    auto [location, valueBuffer, newEntry] = m_core.insert(key);

    // This function does not overwrite previous values.
    if (!newEntry)
        return { Handle(location), false };

    new (valueBuffer) Value(std::forward<Args>(args)...);
    return { Handle(location), true };
}

template <typename Key, typename Value, size_t Order>
template <typename M>
typename BTreeMap<Key, Value, Order>::InsertResult
BTreeMap<Key, Value, Order>::insert_or_assign(const Key& key, M&& obj)
{
    auto [location, valueBuffer, newEntry] = m_core.insert(key);

    if (newEntry)
    {
        new (valueBuffer) Value(std::forward<M>(obj));
        return { Handle(location), true };
    }
    else
    {
        Value& prevValue = *reinterpret_cast<Value*>(valueBuffer);
        prevValue = std::forward<M>(obj);
        return { Handle(location), false };
    }
}

// ------------------------------------------------------------
// Búsqueda
// ------------------------------------------------------------
template <typename Key, typename Value, size_t Order>
typename BTreeMap<Key, Value, Order>::Handle
BTreeMap<Key, Value, Order>::find(const Key& key) const
{
    return Handle{ m_core.find_first(key) };
}

// ------------------------------------------------------------
// Operator []
// ------------------------------------------------------------

// Definición operator[]
template <typename Key, typename Value, size_t Order>
Value& BTreeMap<Key, Value, Order>::operator[](const Key& key)
{
    auto [location, valueBuffer, newEntry] = m_core.insert(key);

    if (newEntry)
        return *new (valueBuffer) Value{};
    else
        return *reinterpret_cast<Value*>(valueBuffer);
}

// Devuelve referencia const a Value existente, o lanza si no está.
template <typename Key, typename Value, size_t Order>
const Value& BTreeMap<Key, Value, Order>::at(const Key& key) const
{
    auto h = find(key);

    if (!h)
        throw std::out_of_range("BTreeMap: key not found");
    else
        return h.value();
}

// ------------------------------------------------------------
// Comparación
// ------------------------------------------------------------
template <typename Key, typename Value, size_t Order>
std::strong_ordering operator<=>(
    const BTreeMap<Key, Value, Order>& lhs
    , const BTreeMap<Key, Value, Order>& rhs
)
{
    auto rhs_range = rhs.begin();

    for (const auto& lhs_entry: lhs)
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

template <typename Key, typename Value, size_t Order>
bool operator==(const BTreeMap<Key, Value, Order>& lhs, const BTreeMap<Key, Value, Order>& rhs)
{
    return (lhs <=> rhs) == 0;
}
