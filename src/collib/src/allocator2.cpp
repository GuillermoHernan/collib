#include "allocator2.h"
#include <malloc.h>

namespace coll
{
	struct MallocAllocator : public IAllocator
	{
		SAllocResult alloc(size_t bytes, size_t) override
		{
			return { malloc(bytes), bytes };
		}

		size_t tryExpand(size_t, void*) override
		{
			return 0;
		}

		void free(void* buffer) override
		{
			::free(buffer);
		}
	};

	IAllocator& defaultAllocator()
	{
		static MallocAllocator sillyMallocAllocator;

		return sillyMallocAllocator;
	}
} //namespace coll