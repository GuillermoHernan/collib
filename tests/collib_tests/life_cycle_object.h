#pragma once

#include <cassert>
#include <compare>

// Test utility object whose main purpose is to chack that constructors / destructors / copiers / movers
// are called right within containers.
class LifeCycleObject
{
public:
    static int default_constructed;
    static int copy_constructed;
    static int move_constructed;
    static int copy_assigned;
    static int move_assigned;
    static int destructed;

private:
    bool m_moved = false;
    int m_value;

public:
    LifeCycleObject() noexcept
        : m_value(0)
        , m_moved(false)
    {
        ++default_constructed;
    }

    LifeCycleObject(int v) noexcept
        : m_value(v)
        , m_moved(false)
    {
        ++default_constructed;
    }

    LifeCycleObject(const LifeCycleObject& other) noexcept
        : m_value(other.m_value)
        , m_moved(false)
    {
        assert(!other.m_moved && "Try to assign from a moved object");
        ++copy_constructed;
    }

    LifeCycleObject(LifeCycleObject&& other) noexcept
    {
        assert(!other.m_moved && "Try to move an already moved object");
        m_value = other.m_value;
        other.m_moved = true;
        m_moved = false;
        ++move_constructed;
    }

    LifeCycleObject& operator=(const LifeCycleObject& other) noexcept
    {
        if (this != &other)
        {
            assert(!other.m_moved && "Try to assign from a moved object");
            m_value = other.m_value;
            m_moved = false;
            ++copy_assigned;
        }
        return *this;
    }

    LifeCycleObject& operator=(LifeCycleObject&& other) noexcept
    {
        if (this != &other)
        {
            assert(!other.m_moved && "Try to move from an already moved object.");
            m_value = other.m_value;
            other.m_moved = true;
            m_moved = false;
            ++move_assigned;
        }
        return *this;
    }

    ~LifeCycleObject() noexcept { ++destructed; }

    auto operator<=>(const LifeCycleObject& other) const = default;

    static void reset_counters() noexcept
    {
        default_constructed = 0;
        copy_constructed = 0;
        move_constructed = 0;
        copy_assigned = 0;
        move_assigned = 0;
        destructed = 0;
    }

    static bool all_destroyed() noexcept
    {
        return destructed == (default_constructed + copy_constructed + move_constructed);
    }

    int value() const noexcept { return m_value; }
};

namespace std
{
template <>
struct hash<LifeCycleObject>
{
    std::size_t operator()(const LifeCycleObject& obj) const noexcept
    {
        return std::hash<int>()(obj.value());
    }
};
}
