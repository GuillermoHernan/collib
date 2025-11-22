#pragma once

#include <concepts>
#include <cstddef>
#include <iterator>

namespace coll
{
    template<typename T>
    concept HasSize = requires(T a)
    {
        { a.size() } -> std::convertible_to<std::size_t>;
    };

    template<typename I>
    concept SimpleIterator = requires(I it) 
    {
        { *it };
        { ++it } -> std::same_as<I&>;
    };

    template<typename S, typename I>
    concept SimpleSentinelFor = requires(S s, I i)
    {
        { s == i } -> std::convertible_to<bool>;
        { s != i } -> std::convertible_to<bool>;
    };

    template<typename T>
    concept Range = requires(T t)
    {
        { std::begin(t) } -> SimpleIterator;
        { std::end(t) } -> SimpleSentinelFor<decltype(std::begin(t))>;
    };

}// namespace coll