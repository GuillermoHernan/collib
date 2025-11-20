#pragma once

#include <cstddef>
#include <limits>

namespace coll
{
	// A collection of elements contigous in memory. Non-owning, other mechanism should be used to
	// manage the lifetime of elements.
	// Very similar to STL version, but with additions to work as a range.
	template <typename Item>
	class span
	{
	public:
		using value_type = typename Item;
		using element_type = typename Item;
		using size_type = std::size_t;
		using difference_type = std::ptrdiff_t;
		using pointer = Item*;
		using reference = Item&;
		using iterator = span<Item>;

		struct Sentinel {};

		span() : m_begin(nullptr), m_end(nullptr)
		{
		}

		span(pointer start, size_type len)
			: m_begin(start)
			, m_end(start + len)
		{
			if (start == nullptr || len == 0)
				m_begin = m_end = nullptr;
		}

		span begin()const { return *this; }
		Sentinel end()const { return Sentinel(); }

		bool empty()const { return m_begin == m_end; }
		size_type size()const { return m_end - m_begin; }

		pointer data()const { return m_begin; }

		reference front()const
		{
			assert(!empty());
			return *m_begin;
		}

		reference operator*()const { return front(); }
		reference back()const
		{
			assert(!empty());
			return *(m_end - 1);
		}

		reference at(size_type i)const
		{
			assert(i < size());
			return m_begin[i];
		}

		reference operator[](size_type i)const { return at(i); };

		//bool operator!=(Sentinel) const { return !empty(); }
		bool operator==(Sentinel) const { return empty(); }

		span& operator++()
		{
			assert(m_begin < m_end);
			++m_begin;
			return *this;
		}

		span operator++(int)
		{
			span old(*this);
			++(*this);
			return old;
		}

		span first(size_t len)const
		{
			return span(m_begin, len <= size() ? len : size());
		}

		span last(size_t len)const
		{
			if (len > size())
				len = size();

			return span(m_end - len, len);
		}

		span subspan(size_t offset, size_t count = std::numeric_limits<size_t>())
		{
			if (offset >= size())
				return span();

			if (count + offset > size())
				count = size() - offset;

			return span(m_begin + offset, count);
		}

	private:
		pointer m_begin;
		pointer m_end;
	};

	template <typename Item>
	span<Item> make_span(Item* start, size_t count)
	{
		return span<Item>(start, count);
	}

	template <typename Item>
	span<Item> make_span(Item* start, Item* end)
	{
		assert(end >= start);
		return span<Item>(start, end - start);
	}

}// namespace coll