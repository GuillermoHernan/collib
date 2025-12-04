#include "pch-collib-tests.h"

#include "life_cycle_object.h"
#include "mem_check_fixture.h"

static bool checkLeaks(const coll::DebugAllocator& dalloc)
{
    if (dalloc.liveAllocationsCount() > 0)
    {
        std::ostringstream message;
        message << "Memory leaks detected:\n";
        dalloc.reportLiveAllocations(message);
        WARN(message.str());
    }

    return dalloc.liveAllocationsCount() == 0;
}

MemCheckFixture::MemCheckFixture()
    : m_holder(m_dalloc)
{
    LifeCycleObject::reset_counters();
}

MemCheckFixture::~MemCheckFixture()
{
    CHECK(checkLeaks(m_dalloc));
    CHECK(LifeCycleObject::all_destroyed());
}
