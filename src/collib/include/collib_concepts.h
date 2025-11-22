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

    template<typename T>
    concept Range = requires(T t)
    {
        { std::begin(t) } -> std::input_iterator;
        { std::end(t) } -> std::sentinel_for<decltype(std::begin(t))>;
    };

}// namespace coll