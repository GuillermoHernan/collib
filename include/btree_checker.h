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

#include "bmap.h"

#include <iostream>
#include <string>
#include <vector>

namespace coll
{
using ErrorReport = std::vector<std::string>;

template <typename Key, BTreeCoreParams Params>
class BTreeCoreChecker
{
public:
    using CoreType = BTreeCore<Key, Params>;

    BTreeCoreChecker(const CoreType& core)
        : m_core(core)
    {
    }

    const ErrorReport& check()
    {
        m_errors.clear();

        if (m_core.m_root == nullptr)
            return m_errors;

        try
        {
            auto leftMost = m_core.leftmost_leaf();
            auto rightMost = m_core.rightmost_leaf();

            require(leftMost != nullptr, "Leftmost leaf cannot be NULL");
            require(rightMost != nullptr, "Rightmost leaf cannot be NULL");

            checkLeaf(*leftMost);
            checkLeaf(*rightMost);

            if (!m_errors.empty())
                return m_errors;

            recursiveBoundsCheck(
                leftMost->key(0),
                rightMost->key(rightMost->count() - 1),
                *m_core.m_root,
                0
            );

            checkLeafChain();
        }
        catch (const Abort&)
        {
            assert(!m_errors.empty() && "There should be some errors if aborted!");
        }

        return m_errors;
    }

    void print(std::ostream& out) const
    {
        unsigned level = 0;

        if (m_core.m_root == nullptr)
            out << "<empty>";
        else
            printNode(out, m_core.m_root, 0);
    }

    // Imprime una clave: usa << si existe, o muestra el tipo si no.
    static std::ostream& printKey(std::ostream& os, const Key& key)
    {
        if constexpr (requires(std::ostream& s, const Key& k) { s << k; })
            os << key;
        else
            os << "[" << typeid(Key).name() << "]";

        return os;
    }

    std::string keyToString(const Key& key)
    {
        std::ostringstream str;
        printKey(str, key);
        return str.str();
    }

private:
    void printNode(std::ostream& out, const CoreType::Node* node, unsigned level) const
    {
        const auto height = m_core.m_height;

        if (level == height - 1)
        {
            const auto* leaf = static_cast<const CoreType::NodeLeaf*>(node);
            out << indent(level) << "[\n";
            for (count_t i = 0; i < leaf->count(); ++i)
            {
                out << indent(level + 1);
                printKey(out, leaf->key(i)) << ": ...\n";
            }
            out << indent(level) << "]\n";
        }
        else
        {
            const auto* internal = static_cast<const CoreType::NodeInternal*>(node);
            out << indent(level) << "{\n";

            printNode(out, internal->children[0], level + 1);
            for (count_t i = 0; i < internal->count(); ++i)
            {
                out << indent(level + 1) << "(";
                printKey(out, internal->key(i)) << ")\n";
                printNode(out, internal->children[i + 1], level + 1);
            }
            out << indent(level) << "}\n";
        }
    }

    static std::string indent(unsigned level) { return std::string(level * 2, ' '); }

    void checkLeaf(const typename CoreType::NodeLeaf& leaf)
    {
        // 1. La hoja no debe estar vacía
        check(leaf.count() > 0, "Leaf node is empty");

        // 2. Las claves deben estar ordenadas estrictamente crecientes
        for (count_t i = 1; i < leaf.count(); ++i)
        {
            if (!(leaf.key(i - 1) < leaf.key(i)))
            {
                check(false, "Leaf keys are not strictly ordered");
                break; // Evitar exceso de reports repetidos
            }
        }

        // 3. Coherencia de punteros prev/next
        if (leaf.prev)
            check(leaf.prev->next == &leaf, "Leaf prev->next does not match current leaf");
        if (leaf.next)
            check(leaf.next->prev == &leaf, "Leaf next->prev does not match current leaf");
    }

    void recursiveBoundsCheck(
        const Key& minKey,
        const Key& maxKey,
        const typename CoreType::Node& node,
        unsigned level
    )
    {
        const bool isLeaf = (level == m_core.m_height - 1);

        if (isLeaf)
        {
            // Comprobación de nodo hoja
            const auto& leaf = static_cast<const typename CoreType::NodeLeaf&>(node);
            require(leaf.count() > 0, "Empty leaf during recursive check");
            checkLeaf(leaf);

            // Chequeo de rango global
            check(
                !(leaf.key(0) < minKey),
                "Leaf key (",
                keyToString(leaf.key(0)),
                ") below minimum bound (",
                keyToString(minKey),
                ")"
            );
            check(
                !(maxKey < leaf.key(leaf.count() - 1)),
                "Leaf key (",
                keyToString(leaf.key(leaf.count() - 1)),
                ") above or equal maximum bound (",
                keyToString(maxKey),
                ")"
            );

            return;
        }

        // Nodo interno
        const auto& internal = static_cast<const typename CoreType::NodeInternal&>(node);

        require(internal.count() > 0, "Internal node must have at least one key");

        // Verifica orden de claves internas local
        for (count_t i = 1; i < internal.count(); ++i)
        {
            if (!(internal.key(i - 1) < internal.key(i)))
                check(false, "Internal node keys not strictly ordered");
        }

        // Chequea recursivamente los hijos.
        // Cada hijo debe tener claves dentro del rango de separación.
        for (count_t i = 0; i <= internal.count(); ++i)
        {
            const Key& childMin = (i == 0) ? minKey : internal.key(i - 1);
            const Key& childMax = (i == internal.count()) ? maxKey : internal.key(i);

            typename CoreType::Node* child = internal.children[i];
            require(child != nullptr, "Internal node has null child pointer");

            recursiveBoundsCheck(childMin, childMax, *child, level + 1);
        }
    }

    bool checkLeafChain()
    {
        bool ok = true;

        const typename CoreType::NodeLeaf* current = m_core.leftmost_leaf();
        require(current != nullptr, "Leftmost leaf is null during leaf-chain check");

        Key lastKey = current->key(0);
        count_t leafCounter = 0;

        while (current)
        {
            for (count_t i = 0; i < current->count(); ++i)
            {
                if (lastKey <= current->key(i))
                    lastKey = current->key(i);
                else
                    ok &= check(false, "Global leaf order violation across leaves");
            }

            const typename CoreType::NodeLeaf* next = current->next;
            if (next)
            {
                ok &= check(next->prev == current, "Broken linkage between leaves");
                ok &= check(
                    current->key(current->count() - 1) < next->key(0),
                    "Key range overlap between adjacent leaves"
                );
            }

            current = next;
            ++leafCounter;
        }

        check(leafCounter > 0, "No leaves found in leaf chain traversal");
        return ok;
    }

    template <typename... Args>
    static std::string formatMessage(const Args&... args)
    {
        std::ostringstream oss;
        ((oss << args), ...);
        return oss.str();
    }

    template <typename... Args>
    void require(bool condition, const Args&... msgArgs)
    {
        if (!check(condition, msgArgs...))
            throw Abort();
    }

    template <typename... Args>
    bool check(bool condition, const Args&... msgArgs)
    {
        if (!condition)
            m_errors.emplace_back(formatMessage(msgArgs...));
        return condition;
    }

private:
    struct Abort
    {
    };

    const CoreType& m_core;
    ErrorReport m_errors;
};

template <typename Key, typename Value, byte_size Order>
class BTreeChecker
{
public:
    using MapType = bmap<Key, Value, Order>;
    using CoreCheckerType = BTreeCoreChecker<Key, MapType::configure()>;

    BTreeChecker(const MapType& map)
        : m_core(map.m_core)
    {
    }

    ErrorReport check() { return m_core.check(); }

    void print(std::ostream& out) const { m_core.print(out); }

private:
    CoreCheckerType m_core;
};

template <typename Key, typename Value, byte_size Order>
BTreeChecker<Key, Value, Order> makeBtreeChecker(const bmap<Key, Value, Order>& map)
{
    return BTreeChecker<Key, Value, Order>(map);
}
} // namespace coll