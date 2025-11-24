#pragma once

#include "allocator2.h"

class MemCheckFixture
{
public:
    MemCheckFixture();
    ~MemCheckFixture();

private:
    coll::DebugAllocator m_dalloc;
    coll::AllocatorHolder m_holder;

};
