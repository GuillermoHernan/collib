#include "pch-collib-tests.h"

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

MemCheckFixture::~MemCheckFixture()
{
    CHECK(checkLeaks(m_dalloc));
}

