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
		using const_pointer = const Item*;
		using const_reference = const Item&;
		using const_iterator = span<Item>;

		struct Sentinel {};

		span() : m_begin(nullptr), m_end(nullptr)
		{
		}

		span(const_pointer start, size_type len)
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

		const_pointer data()const { return m_begin; }

		const_reference front()const
		{
			assert(!empty());
			return *m_begin;
		}

		const_reference operator*()const { return front(); }
		const_reference back()const
		{
			assert(!empty());
			return *(m_end - 1);
		}

		const_reference at(size_type i)const
		{
			assert(i < size());
			return m_begin[i];
		}

		const_reference operator[](size_type i)const { return at(i); };

		bool operator!=(Sentinel) const { return !empty(); }
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
		const_pointer m_begin;
		const_pointer m_end;
	};

	template <typename Item>
	span<Item> make_span(const Item* start, size_t count)
	{
		return span<Item>(start, count);
	}

	template <typename Item>
	span<Item> make_span(const Item* start, const Item* end)
	{
		assert(end >= start);
		return span<Item>(start, end - start);
	}

}// namespace coll