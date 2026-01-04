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
#include "span.h"
#include <cassert>
#include <string>
#include <vector>

using namespace coll;

TEST_CASE("Pruebas básicas de span", "[span]")
{
    int array[] = {1, 2, 3, 4, 5};
    span<int> s(array, 5);

    SECTION("span no vacío y tamaño correcto")
    {
        REQUIRE(!s.empty());
        REQUIRE(s.size() == 5);
        REQUIRE(s.data() == array);
    }

    SECTION("Acceso front, back y operador []")
    {
        REQUIRE(s.front() == 1);
        REQUIRE(s.back() == 5);
        for (count_t i = 0; i < s.size(); ++i)
        {
            REQUIRE(s[i] == array[i]);
            REQUIRE(s.at(i) == array[i]);
        }
    }

    SECTION("Empty span desde nullptr o tamaño 0")
    {
        span<int> empty1(nullptr, 0);
        REQUIRE(empty1.empty());
        REQUIRE(empty1.size() == 0);

        span<int> empty2(nullptr, 10);
        REQUIRE(empty2.empty());
        REQUIRE(empty2.size() == 0);

        span<int> empty3;
        REQUIRE(empty3.empty());
        REQUIRE(empty3.size() == 0);
    }

    SECTION("Iteración mediante range for y comparación con Sentinel")
    {
        count_t count = 0;
        for (const auto& value : s)
        {
            REQUIRE(value == array[count++]);
        }
        REQUIRE(count == s.size());

        span<int> empty(nullptr, 0);
        REQUIRE(empty == empty.end());
        REQUIRE(!(s == s.end()));
        REQUIRE(s != s.end());
        REQUIRE(!(empty != empty.end()));
    }

    SECTION("Incremento de iterador span")
    {
        auto it = s.begin();
        REQUIRE(*it == 1);
        ++it;
        REQUIRE(*it == 2);
        it++;
        REQUIRE(*it == 3);
    }

    SECTION("Métodos first, last y subspan")
    {
        auto first3 = s.first(3);
        REQUIRE(first3.size() == 3);
        REQUIRE(first3[0] == 1);
        REQUIRE(first3[2] == 3);

        auto last2 = s.last(2);
        REQUIRE(last2.size() == 2);
        REQUIRE(last2[0] == 4);
        REQUIRE(last2[1] == 5);

        auto sub = s.subspan(1, 3);
        REQUIRE(sub.size() == 3);
        REQUIRE(sub[0] == 2);
        REQUIRE(sub[2] == 4);

        auto subExceed = s.subspan(10, 5);
        REQUIRE(subExceed.empty());

        auto subCountExceed = s.subspan(3, 10);
        REQUIRE(subCountExceed.size() == 2);
        REQUIRE(subCountExceed[0] == 4);
        REQUIRE(subCountExceed[1] == 5);
    }

    SECTION("Métodos first y last piden más elementos que el tamaño")
    {
        int smallArray[] = {10, 20, 30};
        span<int> smallSpan(smallArray, 3);

        auto first5 = smallSpan.first(5);
        REQUIRE(first5.size() == 3);
        REQUIRE(first5[0] == 10);
        REQUIRE(first5[2] == 30);

        auto last10 = smallSpan.last(10);
        REQUIRE(last10.size() == 3);
        REQUIRE(last10[0] == 10);
        REQUIRE(last10[2] == 30);

        auto first0 = smallSpan.first(0);
        REQUIRE(first0.empty());

        auto last0 = smallSpan.last(0);
        REQUIRE(last0.empty());
    }

    SECTION("Pruebas de make_span")
    {
        int array[] = {10, 20, 30, 40};

        SECTION("make_span con puntero y tamaño")
        {
            auto sp = make_span(array, 3);
            REQUIRE(sp.size() == 3);
            REQUIRE(sp.front() == 10);
            REQUIRE(sp.back() == 30);
        }

        SECTION("make_span con dos punteros (inicio, fin)")
        {
            auto sp = make_span(array, array + 4);
            REQUIRE(sp.size() == 4);
            REQUIRE(sp.front() == 10);
            REQUIRE(sp.back() == 40);
        }

        SECTION("make_span con punteros iguales (span vacío)")
        {
            auto sp = make_span(array + 2, array + 2);
            REQUIRE(sp.empty());
            REQUIRE(sp.size() == 0);
        }
    }
}

TEST_CASE("Reversed span tests", "[span][reversed]")
{
    int array[] = {1, 2, 3, 4, 5};
    span<int> s(array, 5);

    SECTION("Reverse walk")
    {
        int prev = std::numeric_limits<int>::max();

        for (int item : s.rbegin())
        {
            CHECK(prev > item);
            prev = item;
        }
    }

    SECTION("Reversed subspan")
    {
        rspan<int> rev = s.rbegin().subspan(2, 2);
        CHECK(rev.size() == 2);
        CHECK(rev[0] == 3);
        CHECK(rev[1] == 2);

        rev = s.subspan(2, 2).rbegin();
        CHECK(rev.size() == 2);
        CHECK(rev[0] == 4);
        CHECK(rev[1] == 3);
    }

    SECTION("Double reversion is the original order")
    {
        int index = 0;
        for (int item : s.rbegin().rbegin())
            CHECK(item == array[index++]);
    }
}

TEST_CASE("contains tests", "[span][contains]")
{
    int array[] = {1, 2, 3, 4, 5};
    span<int> s(array, 5);

    CHECK(s.contains(s.data()));
    CHECK(s.contains(s.data() + 2));
    CHECK(!s.contains(s.data() + s.size()));
    CHECK(!s.contains(s.data() + s.size() + 100));
    CHECK(!s.contains(s.data() - 1));
}