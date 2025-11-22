#pragma once

#include <cstdint>
#include <exception>

namespace coll
{

	struct SAllocResult
	{
		// nullptr if allocation fails.
		void* buffer;

		// It is guaranteed to be >= requested size. 0 on failure.
		size_t bytes;
	};

	struct IAllocator
	{
		/*
		 * Allocs a new block of memory, which will be at least of the requested size.May be larger.
		 */
		virtual SAllocResult alloc(size_t bytes, size_t align) = 0;

		/* Will try to expand a given allocated block WITHOUT MOVING it.
		 * Returns the new size, which COULD BE lower than the requested size, but at least the original size.
		 * The calling code must check the return value and decide if the new size is enough.
		 * This interface does not support a realloc operation which moves data. That must be handled by
		 * the calling code.
		 */
		virtual size_t tryExpand(size_t bytes, void*) = 0;

		/*
		 * Releases a previously allocated block.
		 */
		virtual void free(void*) = 0;

	protected:
		~IAllocator() = default;
	};

	IAllocator& defaultAllocator();

	template <class T>
	void* checked_alloc(IAllocator& alloc)
	{
		const SAllocResult r = alloc.alloc(sizeof(T), alignof(T));

		if (r.buffer == nullptr)
			throw std::bad_alloc();

		return r.buffer;
	}


}//namespace coll