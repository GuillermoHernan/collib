/*
 * Copyright (c) 2026 Guillermo Hernan Martin
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
 
#include "pch-collib-tests.h"

#include "life_cycle_object.h"
#include "mem_check_fixture.h"

static bool checkLeaks(const coll::DebugLogSink& dalloc)
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
    : m_holder(m_sink)
{
    LifeCycleObject::reset_counters();
}

MemCheckFixture::~MemCheckFixture()
{
    CHECK(checkLeaks(m_sink));
    CHECK(LifeCycleObject::all_destroyed());
}
