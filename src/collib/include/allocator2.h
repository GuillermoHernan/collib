#pragma once

#include <cstdint>
#include <exception>
#include <iosfwd>

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

	/* 
	 * Temporarily sets a given allocator as the current default for the duration of this object's scope.
	 *
	 * - On construction: pushes the supplied allocator onto a stack, making it the active default.
	 * - On destruction: pops the allocator from the stack, restoring the previous default.
	 *
	 * IMPORTANT: AllocatorHolder does not own or manage the lifetime of the allocator it references.
	 * The caller must guarantee that the allocator remains valid for as long as it is set as default.
	 *
	 * If multiple AllocatorHolder instances are active, the most recently created is the current default.
	 * The default allocator is managed as a stack: each holder pushes on entry, pops on exit.
	 */
	class AllocatorHolder
	{
	public:
		AllocatorHolder(IAllocator& alloc);

		~AllocatorHolder() { pop(); }
		void pop();

	private:
		IAllocator* m_alloc;
		size_t m_position;
	};

	template <class T>
	void* checked_alloc(IAllocator& alloc)
	{
		const SAllocResult r = alloc.alloc(sizeof(T), alignof(T));

		if (r.buffer == nullptr)
			throw std::bad_alloc();

		return r.buffer;
	}

	template <typename T, typename... Args>
	T* create(IAllocator& alloc, Args&&... args)
	{
		void* buffer = checked_alloc<T>(alloc);
		return new (buffer) T(std::forward<Args>(args)...);
	}

	template <typename T>
	void destroy(IAllocator& alloc, T* obj)
	{
		if (obj == nullptr)
			return;

		obj->~T();
		alloc.free(obj);
	}

	// DebugAllocator, to help to find and fix memory leaks.
	class DebugAllocator final: public IAllocator
	{
	public:
		explicit DebugAllocator(IAllocator& alloc = defaultAllocator());
		~DebugAllocator();

		// IAllocator implementation
		SAllocResult alloc(size_t bytes, size_t align) override;
		size_t tryExpand(size_t bytes, void* ptr) override;
		void free(void* ptr) override;
		///////////////////////

		size_t liveAllocationsCount() const;

		std::ostream& reportLiveAllocations(std::ostream& os) const;

	private:
		struct SInternal;
		SInternal* m_int;
		IAllocator& m_alloc;

		// Impide copia y asignación para evitar problemas de doble liberación
		DebugAllocator(const DebugAllocator&) = delete;
		DebugAllocator& operator=(const DebugAllocator&) = delete;
	};


}//namespace coll