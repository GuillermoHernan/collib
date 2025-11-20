#pragma once

#include <compare>
#include <concepts>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <utility>
//#include <memory>

#include "span.h"
#include "allocator2.h"

namespace coll
{
    template<typename T>
    concept HasSize = requires(T a)
    {
        { a.size() } -> std::convertible_to<std::size_t>;
    };

    template<typename T>
    concept Range = requires(T t)
    {
        { std::begin(t) } -> std::input_iterator;
        { std::end(t) } -> std::sentinel_for<decltype(std::begin(t))>;
    };

    template<typename Item>
    class darray {
    public:
        // Sub-types
        using value_type = Item;
        using allocator_type = std::allocator<Item>;
        using size_type = size_t;
        using difference_type = std::ptrdiff_t;
        using reference = value_type&;
        using const_reference = const value_type&;
        using pointer = value_type*;
        using const_pointer = const value_type*;
        using iterator = span<Item>;
        using const_iterator = span<const Item>;
        using reverse_iterator = rspan<Item>;
        using const_reverse_iterator = rspan<const Item>;

        // Constructores
        explicit darray(IAllocator& alloc = defaultAllocator())
            : m_allocator(&alloc), m_data(nullptr), m_size(0), m_capacity(0)
        {}

        explicit darray(size_t count, IAllocator& alloc = defaultAllocator());
        darray(size_t count, const Item& value, IAllocator& alloc = defaultAllocator());
        darray(const darray& other);
        darray(darray&& other) noexcept;
        darray(std::initializer_list<Item> init);

        template<Range RangeType>
        darray(const RangeType& range, IAllocator& alloc = defaultAllocator());

        // Destructor
        ~darray();

        // Asignación
        darray& operator=(const darray& other);
        darray& operator=(darray&& other) noexcept;
        darray& operator=(std::initializer_list<Item> ilist);

        // Acceso a elementos
        Item& operator[](size_t index) { return at(index); }
        const Item& operator[](size_t index) const { return at(index); }
        Item& at(size_t index);
        const Item& at(size_t index) const;

        // Comparaciones
        template<Range RangeType>
        auto operator<=>(const RangeType& rhs) const;

        template<Range RangeType>
        bool operator==(const RangeType& rhs) const { return (this->operator<=>(rhs) == 0); }
        
        Item& front() { return at(0); }
        const Item& front() const { return at(0); }
        
        Item& back();
        const Item& back() const;
        Item* data() noexcept { return m_data; }
        const Item* data() const noexcept { return m_data; }

        // Iteradores
        iterator begin() noexcept { return make_span(m_data, m_size); }
        iterator::Sentinel end() noexcept { return {}; }

        const_iterator begin() const noexcept { return span<const Item>(m_data, m_size); }
        const_iterator::Sentinel end() const noexcept { return {}; }
        const_iterator cbegin() const noexcept { return begin(); }
        const_iterator::Sentinel cend() const noexcept { return {}; }

        reverse_iterator rbegin() noexcept { return reverse_iterator(m_data, m_size); }
        reverse_iterator::Sentinel rend() noexcept { return {}; }

        const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(m_data, m_size); }
        const_reverse_iterator::Sentinel rend() const noexcept { return {}; }
        const_reverse_iterator crbegin() const noexcept { return rbegin(); }
        const_reverse_iterator::Sentinel crend() const noexcept { return rend(); }

        // Capacidad
        bool empty() const noexcept { return m_size == 0; }
        size_t size() const noexcept { return m_size; }
        //size_t max_size() const noexcept;
        void reserve(size_t new_cap) { reallocate_if_needed(new_cap); }
        size_t capacity() const noexcept { return m_capacity; }
        //void shrink_to_fit();

        // Modificadores
        void clear() noexcept;

        // Inserciones estándar
        void insert(size_t pos, const Item& value);
        void insert(size_t pos, Item&& value);
        void insert(size_t pos, size_t count, const Item& value);
        void insert(size_t pos, std::initializer_list<Item> ilist);

        template<Range RangeType>
        void insert(size_t pos, const RangeType& range) { return insert_range(pos, range); }

        template<Range RangeType>
        void insert_range(size_t pos, const RangeType& range);

        template<Range RangeType>
        void append_range(const RangeType& range);

        // Emplace
        template<typename... Args>
        Item& emplace_back(Args&&... args);
        template<typename... Args>
        Item& emplace(size_t pos, Args&&... args);

        // Push/pop back
        void push_back(const Item& value);
        void push_back(Item&& value);
        void pop_back();

        // Redimensionar y asignar
        void resize(size_t count);
        void resize(size_t count, const Item& value);
        void swap(darray& other) noexcept;

        void assign(size_t count, const Item& value);
        void assign(std::initializer_list<Item> ilist) { return assign_range(ilist); }

        template<Range RangeType>
        void assign(const RangeType& range) { return assign_range(range); }

        template<Range RangeType>
        void assign_range(const RangeType& range);

    private:
        IAllocator* m_allocator;
        Item* m_data;
        size_t m_size;
        size_t m_capacity;

        void reallocate_if_needed(size_t required_size);
        void shift_right(size_t start_index, size_t count);
    };

    template<typename Item>
    darray<Item>::darray(size_t count, IAllocator& alloc)
        : m_allocator(&alloc), m_data(nullptr), m_size(0), m_capacity(0)
    {
        if (count > 0) 
        {
            reallocate_if_needed(count);
            for (size_t i = 0; i < count; ++i)
                new (m_data + i) Item();
            
            m_size = count;
        }
    }

    template<typename Item>
    darray<Item>::darray(size_t count, const Item& value, IAllocator& alloc)
        : m_allocator(&alloc), m_data(nullptr), m_size(0), m_capacity(0) 
    {
        if (count > 0) 
        {
            reallocate_if_needed(count);
            for (size_t i = 0; i < count; ++i)
                new (m_data + i) Item(value);
            
            m_size = count;
        }
    }

    template<typename Item>
    darray<Item>::darray(const darray& other)
        : m_allocator(other.m_allocator), m_data(nullptr), m_size(0), m_capacity(0) 
    {
        if (other.m_size > 0) 
        {
            reallocate_if_needed(other.m_size);
            for (size_t i = 0; i < other.m_size; ++i)
                new (m_data + i) Item(other.m_data[i]);
            
            m_size = other.m_size;
        }
    }

    template<typename Item>
    darray<Item>::darray(darray&& other) noexcept
        : m_allocator(other.m_allocator)
        , m_data(other.m_data)
        , m_size(other.m_size)
        , m_capacity(other.m_capacity) 
    {
        other.m_data = nullptr;
        other.m_size = 0;
        other.m_capacity = 0;
    }

    template<typename Item>
    darray<Item>::darray(std::initializer_list<Item> init)
        : m_allocator(&defaultAllocator()), m_data(nullptr), m_size(0), m_capacity(0) 
    {
        if (init.size() > 0) 
        {
            reallocate_if_needed(init.size());
            size_t i = 0;
            for (const auto& item : init) 
            {
                new (m_data + i) Item(item);
                ++i;
            }
            m_size = init.size();
        }
    }

    template<typename Item>
    template<Range RangeType>
    darray<Item>::darray(const RangeType& range, IAllocator& alloc)
        : m_allocator(&alloc), m_data(nullptr), m_size(0), m_capacity(0) 
    {
        if constexpr (HasSize<RangeType>) 
        {
            // Usa size() si existe
            const size_t count = range.size();
            
            if (count > 0) 
            {
                reallocate_if_needed(count);
                size_t idx = 0;
                for (const auto& item : range) 
                {
                    new (m_data + idx) Item(item);
                    ++idx;
                }
                m_size = count;
            }
        }
        else 
        {
            for (const auto& item : range)
                this->push_back(item);
        }
    }

    template<typename Item>
    darray<Item>::~darray() 
    {
        clear();
        
        if (m_data) 
        {
            m_allocator->free(m_data);
            m_data = nullptr;
        }
    }

    template<typename Item>
    darray<Item>& darray<Item>::operator=(const darray& other) 
    {
        if (this == &other)
            return *this;

        clear();

        if (m_data && &m_allocator != &other.m_allocator) 
        {
            m_allocator->free(m_data);
            m_data = nullptr;
            m_capacity = 0;
        }

        m_allocator = other.m_allocator;

        if (other.m_size > 0) 
        {
            reallocate_if_needed(other.m_size);
            for (size_t i = 0; i < other.m_size; ++i)
                new (m_data + i) Item(other.m_data[i]);
            
            m_size = other.m_size;
        }
        return *this;
    }

    template<typename Item>
    darray<Item>& darray<Item>::operator=(darray&& other) noexcept
    {
        if (this == &other)
            return *this;

        clear();

        if (m_data)
            m_allocator->free(m_data);

        // Mover recursos
        m_data = other.m_data;
        m_size = other.m_size;
        m_capacity = other.m_capacity;
        m_allocator = other.m_allocator;

        // Dejar otro en estado válido vacío
        other.m_data = nullptr;
        other.m_size = 0;
        other.m_capacity = 0;

        return *this;
    }

    template<typename Item>
    darray<Item>& darray<Item>::operator=(std::initializer_list<Item> ilist)
    {
        clear();

        if (ilist.size() > 0) 
        {
            reallocate_if_needed(ilist.size());
            size_t i = 0;
            for (const auto& item : ilist) 
            {
                new (m_data + i) Item(item);
                ++i;
            }
            m_size = ilist.size();
        }

        return *this;
    }

    template<typename Item>
    void darray<Item>::clear() noexcept
    {
        for (auto& item : *this)
            item.~Item();

        m_size = 0;
    }

    template<typename Item>
    Item& darray<Item>::at(size_t index)
    {
        if (index >= m_size)
            throw std::out_of_range("Index out of range");

        return m_data[index];
    }

    template<typename Item>
    const Item& darray<Item>::at(size_t index)const
    {
        if (index >= m_size)
            throw std::out_of_range("Index out of range");

        return m_data[index];
    }

    template<typename Item>
    template<Range RangeType>
    auto darray<Item>::operator<=>(const RangeType& rhs) const
    {
        auto itB = std::begin(rhs);
        auto endB = std::end(rhs);

        for (const auto& itemA : *this)
        {
            if (itB == endB)
                return std::strong_ordering::greater;

            const auto& itemB = *itB;
            auto cmp = (itemA <=> itemB);

            if (cmp != 0)
                return cmp;
            else
                ++itB;
        }

        if (itB == endB)
            return std::strong_ordering::equal;
        else
            return std::strong_ordering::less;
    }

    template<typename Item>
    Item& darray<Item>::back()
    {
        if (empty())
            throw std::out_of_range("Accessing an empty darray");
        return at(m_size - 1);
    }

    template<typename Item>
    const Item& darray<Item>::back()const
    {
        if (empty())
            throw std::out_of_range("Accessing an empty darray");
        return at(m_size - 1);
    }

    template<typename Item>
    void darray<Item>::push_back(const Item& value)
    {
        reallocate_if_needed(m_size + 1);
        new (m_data + m_size) Item(value);
        ++m_size;
    }

    template<typename Item>
    void darray<Item>::push_back(Item&& value)
    {
        reallocate_if_needed(m_size + 1);
        new (m_data + m_size) Item(std::move(value));
        ++m_size;
    }

    template<typename Item>
    void darray<Item>::pop_back()
    {
        if (m_size == 0)
            throw std::out_of_range("pop_back() called on empty darray");

        m_size--;
        m_data[m_size].~Item();
    }

    template<typename Item>
    template<typename... Args>
    Item& darray<Item>::emplace_back(Args&&... args)
    {
        reallocate_if_needed(m_size + 1);
        new (m_data + m_size) Item(std::forward<Args>(args)...);
        return m_data[m_size++];
    }

    template<typename Item>
    void darray<Item>::insert(size_t pos, const Item& value)
    {
        size_t index = pos;
        reallocate_if_needed(m_size + 1);

        shift_right(index, 1);

        new (m_data + index) Item(value);
        ++m_size;
    }

    template<typename Item>
    void darray<Item>::insert(size_t pos, Item&& value)
    {
        size_t index = pos;
        reallocate_if_needed(m_size + 1);

        shift_right(index, 1);

        new (m_data + index) Item(std::move(value));
        ++m_size;
    }

    template<typename Item>
    void darray<Item>::insert(size_t pos, size_t count, const Item& value)
    {
        size_t index = pos;

        if (count == 0)
            return;

        reallocate_if_needed(m_size + count);

        shift_right(index, count);

        Item* insert_pos = m_data + index;
        for (size_t i = 0; i < count; ++i)
            new (insert_pos + i) Item(value);

        m_size += count;
    }

    template<typename Item>
    template<Range RangeType>
    void darray<Item>::insert_range(size_t pos, const RangeType& range)
    {
        size_t index = pos;

        size_t count = 0;
        if constexpr (HasSize<RangeType>)
            count = range.size();
        else
            count = std::distance(std::begin(range), std::end(range));

        if (count == 0)
            return;

        reallocate_if_needed(m_size + count);
        shift_right(index, count);

        Item* insert_pos = m_data + index;

        size_t i = 0;
        for (const auto& item : range)
        {
            new (insert_pos + i) Item(item);
            ++i;
        }
        m_size += count;

        return;       
    }

    template<typename Item>
    void darray<Item>::insert(size_t pos, std::initializer_list<Item> ilist)
    {
        return insert_range(pos, ilist);
    }

    template<typename Item>
    template<typename... Args>
    Item& darray<Item>::emplace(size_t pos, Args&&... args)
    {
        size_t index = pos;
        reallocate_if_needed(m_size + 1);

        // Hacer espacio para el nuevo elemento
        shift_right(index, 1);

        // Construir el nuevo elemento in-place usando perfect forwarding
        Item* new_item = new (m_data + index) Item(std::forward<Args>(args)...);
        ++m_size;

        return *new_item;
    }

    template<typename Item>
    template<Range RangeType>
    void darray<Item>::append_range(const RangeType& range)
    {
        size_t count = 0;
        if constexpr (HasSize<RangeType>)
            count = range.size();
        else
            count = std::distance(std::begin(range), std::end(range));

        if (count == 0)
            return;

        reallocate_if_needed(m_size + count);

        size_t i = 0;
        for (const auto& item : range)
        {
            new (m_data + m_size + i) Item(item);
            ++i;
        }
        m_size += count;
    }

    template<typename Item>
    void darray<Item>::resize(size_t count)
    {
        if (count < m_size) 
        {
            // Destruir elementos sobrantes
            for (size_t i = count; i < m_size; ++i)
                m_data[i].~Item();
            m_size = count;
        }
        else if (count > m_size) 
        {
            reallocate_if_needed(count);
            // Construir nuevos elementos con valor por defecto
            for (size_t i = m_size; i < count; ++i)
                new (m_data + i) Item();
            m_size = count;
        }
        // si count == m_size, no hacer nada
    }

    template<typename Item>
    void darray<Item>::resize(size_t count, const Item& value)
    {
        if (count < m_size)
            resize(count);
        else if (count > m_size) 
        {
            reallocate_if_needed(count);
            // Construir nuevos elementos con 'value'
            for (size_t i = m_size; i < count; ++i)
                new (m_data + i) Item(value);
            m_size = count;
        }
        // si count == m_size, no hacer nada
    }

    template<typename Item>
    void darray<Item>::swap(darray& other) noexcept
    {
        using std::swap;
        swap(m_allocator, other.m_allocator);
        swap(m_data, other.m_data);
        swap(m_size, other.m_size);
        swap(m_capacity, other.m_capacity);
    }

    template<typename Item>
    void darray<Item>::assign(size_t count, const Item& value)
    {
        clear();
        if (count == 0)
            return;

        reallocate_if_needed(count);
        for (size_t i = 0; i < count; ++i)
            new (m_data + i) Item(value);
        m_size = count;
    }

    template<typename Item>
    template<Range RangeType>
    void darray<Item>::assign_range(const RangeType& rg)
    {
        clear();
        size_t count = 0;
        if constexpr (HasSize<RangeType>)
            count = rg.size();
        else
            count = std::distance(std::begin(rg), std::end(rg));

        if (count == 0)
            return;

        reallocate_if_needed(count);
        size_t i = 0;
        for (const auto& item : rg)
        {
            new (m_data + i) Item(item);
            ++i;
        }
        m_size = count;
    }

    template<typename Item>
    void darray<Item>::shift_right(size_t start_index, size_t count)
    {
        assert(count + m_size <= m_capacity);

        // Mover elementos count posiciones a la derecha a partir de start_index
        size_t destIndex = m_size + count - 1;
        for (size_t srcIndex = m_size; srcIndex > start_index; --destIndex)
        {
            --srcIndex;
            new (m_data + destIndex) Item(std::move(m_data[srcIndex]));
            m_data[srcIndex].~Item();
        }
    }

    template<typename Item>
    void darray<Item>::reallocate_if_needed(size_t required_size) 
    {
        const size_t kInitialCapacity = 4;

        if (required_size <= m_capacity)
            return;

        // Intenta expandir sin mover si la memoria ya está asignada
        if (m_data) 
        {
            const size_t expanded_size_bytes = m_allocator->tryExpand(required_size * sizeof(Item), m_data);
            if (expanded_size_bytes >= required_size * sizeof(Item))
            {
                // La expansión fue exitosa, actualizamos capacidad y salimos
                m_capacity = expanded_size_bytes / sizeof(Item);
                return;
            }
            // Si la expansión fue insuficiente, continúa con realloc normal
        }

        // Nueva capacidad: al menos doble o requerido
        size_t new_capacity = m_capacity == 0 ? kInitialCapacity : m_capacity * 2;
        if (new_capacity < required_size)
            new_capacity = required_size;        

        // Solicita nueva memoria al asignador
        const SAllocResult alloc_result = m_allocator->alloc(new_capacity * sizeof(Item), alignof(Item));
        if (alloc_result.buffer == nullptr)
            throw std::bad_alloc();

        Item* new_data = static_cast<Item*>(alloc_result.buffer);

        // Mover o copiar elementos antiguos al nuevo buffer
        try
        {
            for (size_t i = 0; i < m_size; ++i)
            {
                new (new_data + i) Item(std::move(m_data[i]));
                m_data[i].~Item();
            }
        }
        catch (...)
        {
            // TODO: Destroy elements already copied? But that can throw new exceptions...
            m_allocator->free(alloc_result.buffer);
            throw;
        }

        // Libera memoria antigua
        if (m_data)
            m_allocator->free(m_data);

        m_data = new_data;
        m_capacity = alloc_result.bytes / sizeof(Item);
    }

}// namespace coll