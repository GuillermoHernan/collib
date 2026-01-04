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
 
#pragma once

#include "collib_types.h"

#include <concepts>
#include <cstddef>
#include <iterator>

namespace coll
{
template <typename T>
concept HasSize = requires(T a) {
    { a.size() } -> std::convertible_to<count_t>;
};

template <typename I>
concept SimpleIterator = requires(I it) {
    { *it };
    { ++it } -> std::same_as<I&>;
};

template <typename S, typename I>
concept SimpleSentinelFor = requires(S s, I i) {
    { s == i } -> std::convertible_to<bool>;
    { s != i } -> std::convertible_to<bool>;
};

template <typename T>
concept Range = requires(T t) {
    { std::begin(t) } -> SimpleIterator;
    { std::end(t) } -> SimpleSentinelFor<decltype(std::begin(t))>;
};

} // namespace coll