#pragma once

#include "allocator2.h"
#include "collib_concepts.h"

#include <functional>
#include <optional>

namespace coll
{
// Virtual ranges. To iterate ranges without knowing the backing container.
// Applies dynamic polymorphism to ranges.
template <typename Item>
class vrange
{
public:
    using value_type = Item;
    using const_reference = const value_type&;

    struct Sentinel
    {
    };
    struct IRange;

    vrange()
        : m_inner(emptyInnerRange())
    {
    }

    template <typename RangeType>
    static vrange from(const RangeType& range, IAllocator& alloc = defaultAllocator())
    {
        check_range_compatible<RangeType>();

        vrange newRange;
        newRange.m_inner = make_inner(range, alloc);
        return newRange;
    }

    // WARNING: This is an advanced usage constructor which requires manual memory management.
    // The range will take ownership of the 'IRange' object, and call 'destroy' when finished.
    // IRange implementations must de-initialice AND deallocate memory on its 'destroy' method.
    explicit vrange(IRange* range)
        : m_inner(range)
    {
    }

    vrange(const vrange& rhs)
        : m_inner(rhs.m_inner->clone())
    {
    }
    vrange(vrange&& rhs)
        : m_inner(rhs.m_inner)
    {
        rhs.m_inner = emptyInnerRange();
    }

    vrange& operator=(const vrange& rhs)
    {
        if (this->m_inner == rhs.m_inner)
            return *this;

        m_inner->destroy();

        m_inner = rhs.m_inner->clone();
        return *this;
    }

    vrange& operator=(vrange&& rhs)
    {
        if (this->m_inner == rhs.m_inner)
        {
            rhs.m_inner = emptyInnerRange();
            return *this;
        }

        m_inner->destroy();

        m_inner = rhs.m_inner;
        rhs.m_inner = emptyInnerRange();
        return *this;
    }

    ~vrange() { m_inner->destroy(); }

    const_reference front() const { return m_inner->front(); }
    const_reference operator*() const { return front(); }

    bool empty() const { return m_inner->empty(); }
    vrange begin() const { return *this; }
    Sentinel end() const { return Sentinel(); }

    vrange<Item>
    filter(std::function<bool(const Item&)>&& filterFn, IAllocator& alloc = defaultAllocator()) const;

    template <typename Func>
    auto transform(Func&& mapFn, IAllocator& alloc = defaultAllocator()) const
        -> vrange<decltype(mapFn(std::declval<Item>()))>;

    bool operator==(Sentinel) const { return empty(); }

    vrange& operator++()
    {
        m_inner->next();
        return *this;
    }

    vrange operator++(int)
    {
        const vrange old(*this);
        ++(*this);
        return old;
    }

    struct IRange
    {
        virtual bool empty() const = 0;
        virtual const_reference front() = 0;
        virtual bool next() = 0;

        virtual IRange* clone() = 0;
        virtual void destroy() = 0;

    protected:
        ~IRange() = default;
    };

private:
    static IRange* emptyInnerRange()
    {
        static EmptyRange theEmptyRange;
        return &theEmptyRange;
    }

    template <Range RangeType>
    class InnerRangeImpl final : public IRange
    {
    public:
        using BeginType = decltype(std::declval<const RangeType&>().begin());

        InnerRangeImpl(const RangeType& range, IAllocator& alloc)
            : m_range(range.begin())
            , m_alloc(alloc)
        {
        }

        // IRange Implementation
        bool empty() const override { return m_range.empty(); }
        const_reference front() override { return *m_range; }
        bool next() override { return (++m_range).empty(); }
        IRange* clone() override
        {
            void* buffer = checked_alloc<InnerRangeImpl>(m_alloc);
            return new (buffer) InnerRangeImpl(*this);
        }

        void destroy() override
        {
            IAllocator& alloc(m_alloc);
            this->~InnerRangeImpl();
            alloc.free(this);
        }

    private:
        BeginType m_range;
        IAllocator& m_alloc;
    };

    template <typename Iterator, typename SentinelType>
    class InnerRangeIterators final : public IRange
    {
    public:
        InnerRangeIterators(Iterator begin, SentinelType end, IAllocator& alloc)
            : m_begin(begin)
            , m_end(end)
            , m_alloc(alloc)
        {
        }

        // IRange Implementation
        bool empty() const override { return m_begin == m_end; }
        const_reference front() override { return *m_begin; }
        bool next() override
        {
            ++m_begin;
            return empty();
        }

        IRange* clone() override
        {
            void* buffer = checked_alloc<InnerRangeIterators>(m_alloc);
            return new (buffer) InnerRangeIterators(*this);
        }

        void destroy() override
        {
            IAllocator& alloc(m_alloc);
            this->~InnerRangeIterators();
            alloc.free(this);
        }

    private:
        Iterator m_begin;
        SentinelType m_end;
        IAllocator& m_alloc;
    };

    class EmptyRange final : public IRange
    {
    public:
        bool empty() const override { return true; }
        const_reference front() override
        {
            throw std::out_of_range("Trying to read an item from an empty range");
        }
        bool next() override { return false; }
        IRange* clone() override { return this; }
        void destroy() override { }
    };

    template <Range RangeType>
    static IRange* make_inner(const RangeType& range, IAllocator& alloc)
    {
        using BeginType = decltype(std::declval<const RangeType&>().begin());

        if constexpr (Range<BeginType>)
        {
            // begin() devuelve un rango -> usar InnerRangeImpl
            using InnerType = InnerRangeImpl<RangeType>;
            void* buffer = checked_alloc<InnerType>(alloc);
            return new (buffer) InnerType(range, alloc);
        }
        else
        {
            // begin() devuelve iteradores -> usar InnerRangeIterators
            using IteratorType = decltype(range.begin());
            using SentinelType = decltype(range.end());
            using InnerTypeIter = InnerRangeIterators<IteratorType, SentinelType>;
            void* buffer = checked_alloc<InnerTypeIter>(alloc);
            return new (buffer) InnerTypeIter(range.begin(), range.end(), alloc);
        }
    }

    template <Range RangeType>
    static constexpr void check_range_compatible()
    {
        using RangeItem = typename std::remove_cvref_t<RangeType>::value_type;
        static_assert(
            std::derived_from<RangeItem, Item> || std::is_same_v<RangeItem, Item>,
            "Range is incompatible with item type"
        );
    }

    IRange* m_inner;
};

template <Range RangeType>
vrange<typename RangeType::value_type>
make_range(const RangeType& range, IAllocator& alloc = defaultAllocator())
{
    return vrange<typename RangeType::value_type>::from(range, alloc);
}

template <typename Item>
vrange<Item> vrange<Item>::filter(std::function<bool(const Item&)>&& filterFn, IAllocator& alloc) const
{
    using FilterFunction = std::function<bool(const Item&)>;

    // What this function does is to create an instance of this class an put it inside a vrange.
    // Then this class will do the filtering while walking the original range.
    struct FilterState final : public IRange
    {
        FilterFunction m_filterFn;
        vrange<Item> m_input;
        IAllocator& m_alloc;

        FilterState(const vrange<Item>& input, FilterFunction&& filterFn, IAllocator& alloc)
            : m_filterFn(std::move(filterFn))
            , m_input(input)
            , m_alloc(alloc)
        {
            // Need to skip (potential) filtered items at the beginning.
            if (!m_input.empty() && !m_filterFn(*m_input))
                next();
        }

        bool empty() const override { return m_input.empty(); }
        const_reference front() override { return m_input.front(); }

        bool next() override
        {
            ++m_input;

            for (; !m_input.empty(); ++m_input)
            {
                if (m_filterFn(*m_input))
                    break; // found.
            }

            return empty();
        }

        IRange* clone() override { return create<FilterState>(m_alloc, *this); }

        void destroy() override { ::destroy(m_alloc, this); }
    };

    FilterState* state = create<FilterState>(alloc, *this, std::move(filterFn), alloc);
    return vrange(state);
}

template <class Item>
template <typename Func>
auto vrange<Item>::transform(Func&& mapFn, IAllocator& alloc) const
    -> vrange<decltype(mapFn(std::declval<Item>()))>
{
    using OutputItem = decltype(mapFn(std::declval<Item>()));
    using MapFunction = Func;
    using IBaseRange = vrange<OutputItem>::IRange;

    struct TransformState final : public IBaseRange
    {
        MapFunction m_mapFn;
        vrange<Item> m_input;
        mutable std::optional<OutputItem> m_output;
        IAllocator& m_alloc;

        TransformState(const vrange<Item>& input, MapFunction&& mapFn, IAllocator& alloc)
            : m_mapFn(std::move(mapFn))
            , m_input(input)
            , m_alloc(alloc)
        {
        }

        bool empty() const override { return m_input.empty(); }
        const OutputItem& front() override
        {
            m_output.emplace(m_mapFn(*m_input));
            return m_output.value();
        }

        bool next() override
        {
            ++m_input;
            return empty();
        }

        IRange* clone() override { return create<TransformState>(m_alloc, *this); }

        void destroy() override { ::destroy(m_alloc, this); }
    };

    IBaseRange* state = create<TransformState>(alloc, *this, std::move(mapFn), alloc);
    return vrange<OutputItem>(state);
}

} // namespace coll