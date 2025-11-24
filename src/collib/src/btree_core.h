#pragma once

#include "allocator2.h"
#include <assert.h>
#include <cstddef>
#include <optional>

namespace coll
{
    struct BTreeCoreParams
    {
        size_t Order = 4;
        size_t ValueSize = 0;
        size_t ValueAlign = 1; // Valor mínimo por defecto
        void (*DestroyValueFn)(void*) = nullptr;
        void (*MoveValueFn)(void* dest, void* src) = nullptr;
    };

    template<typename Key, BTreeCoreParams Params>
    class BTreeCore
    {
    public:
        static constexpr size_t ValueSize = Params.ValueSize;
        static constexpr size_t ValueAlign = Params.ValueAlign;
        static constexpr size_t Order = Params.Order;

        struct InsertResult;
        class Handle;
        class Range;
        class InvRange;

        BTreeCore(IAllocator& alloc);
        ~BTreeCore();

        BTreeCore(const BTreeCore& rhs)=delete;
        BTreeCore(BTreeCore&& rhs) noexcept;

        BTreeCore& operator=(const BTreeCore& rhs)=delete;
        BTreeCore& operator=(BTreeCore&& rhs) noexcept;

        size_t size() const { return m_size; }
        bool empty() const { return m_size == 0; }

        IAllocator& allocator()const { return *m_alloc; }

        Handle find_first(const Key& key) const
        {
            const Handle h = lower_bound(key);

            if (!h.has_value() || h.key() != key)
                return {};
            else
                return h;
        }

        Handle lower_bound(const Key& key) const;
        Range range(const Key& key) const;
        Range range(const Key& keyLeft, const Key& keyRight) const;
        bool contains(const Key& key) const { return find_first(key).has_value(); }
        size_t count(const Key& key) const;

        Range begin() const;
        Range end() const { return Range(); }

        InvRange rbegin() const;
        InvRange rend() const { return InvRange(); }

        InsertResult insert(const Key& key);
        bool erase(const Key& key);

        void clear();

        class Handle
        {
        public:
            Handle() : m_leaf(nullptr), m_index(0) {}
            Handle(typename BTreeCore::NodeLeaf* leaf, size_t index)
                : m_leaf(leaf), m_index(index)
            {}

            const Key& key()const { return m_leaf->key(m_index); }
            const void* value()const { return m_leaf->values[m_index].data; }

            bool has_value() const { return m_leaf != nullptr; }

            bool operator!=(const Handle& rhs) const
            {
                return m_leaf != rhs.m_leaf || m_index != rhs.m_index;
            }
            bool operator ==(const Handle& rhs) const { return !(operator!=(rhs)); }

            operator bool()const { return has_value(); }

        protected:
            typename BTreeCore::NodeLeaf* m_leaf;
            size_t m_index;

            friend class BTreeCore;
        };// class Handle

        struct InsertResult
        {
            Handle location;
            void* valueBuffer;
            bool newEntry;
        };

        class Range
        {
        public:
            Range() 
                : m_leaf(nullptr)
                , m_endLeaf(nullptr)
                , m_endIndex(0) 
                , m_index(0) 
            {}
            Range(typename BTreeCore::NodeLeaf* leaf, size_t index)
                : m_leaf(leaf)
                , m_endLeaf(nullptr)
                , m_index(index)
                , m_endIndex(0)
            {}
            Range(
                typename BTreeCore::NodeLeaf* leaf
                , size_t index
                , typename BTreeCore::NodeLeaf* endLeaf
                , size_t endIndex
            )
                : m_leaf(leaf)
                , m_endLeaf(endLeaf)
                , m_index(index)
                , m_endIndex(0)
            {}

            const Key& key()const { return m_leaf->key(m_index); }
            const void* value()const { return m_leaf->values[m_index].data; }

            bool empty() const { return m_leaf == nullptr; }

            Range& operator++()
            {
                if (!m_leaf)
                    return *this;

                ++m_index;
                if (m_index >= m_leaf->count())
                {
                    m_leaf = m_leaf->next;
                    m_index = 0;
                }
                return *this;
            }

            Range operator++(int)
            {
                Range old(*this);
                ++(*this);
                return old;
            }

        protected:
            typename BTreeCore::NodeLeaf* m_leaf;
            typename BTreeCore::NodeLeaf* m_endLeaf;
            size_t m_index;
            size_t m_endIndex;

            friend class BTreeCore;
        }; // Class Range

        class InvRange : public Range
        {
        public:
            InvRange() = default;
            InvRange(typename BTreeCore::NodeLeaf* leaf, size_t index)
                : Range(leaf, index)
            {
            }

            InvRange& operator++()
            {
                if (this->m_leaf == nullptr)
                    return *this;

                if (this->m_index == 0)
                {
                    this->m_leaf = this->m_leaf->prev;
                    if (this->m_leaf != nullptr)
                        this->m_index = this->m_leaf->count() - 1;
                }
                else
                    --this->m_index;

                return *this;
            }

            InvRange operator++(int)
            {
                InvRange old(*this);
                ++(*this);
                return old;
            }
        };// class InvRange

    private:
        template<typename Key, BTreeCoreParams Params>
        friend class BTreeCoreChecker;

        union alignas(ValueAlign) AlignedValueStorage
        {
            std::byte data[ValueSize];
        };

        class Node 
        {
        public:
            ~Node()
            {
                resize_keys(0);
            }

            const Key& key(size_t index)const
            {
                assert(index < m_count);
                return reinterpret_cast<const Key*>(m_key_store)[index];
            }
            size_t count()const { return m_count; }

            Key change_key(size_t index, const Key& key)
            {
                assert(index < m_count);
                auto* keys = reinterpret_cast<Key*>(m_key_store);
                Key old = keys[index];
                keys[index] = key;
                return old;
            }

        protected:
            void add_key(const Key& key)
            {
                assert(m_count < Order);
                auto* keys = reinterpret_cast<Key*>(m_key_store);
                new (keys + m_count) Key(key);
                ++m_count;
            }

            void add_key(Key&& key)
            {
                assert(m_count < Order);
                auto* keys = reinterpret_cast<Key*>(m_key_store);
                new (keys + m_count) Key(std::move(key));
                ++m_count;
            }

            void insert_key(size_t index, const Key& key)
            {
                assert(m_count < Order);
                assert(index <= m_count);

                if (index == m_count)
                    return add_key(key);

                auto* keys = reinterpret_cast<Key*>(m_key_store);

                new (keys + m_count) Key(std::move(keys[m_count-1]));
                for (size_t j = m_count - 1; j > index; --j)
                    keys[j] = std::move(keys[j - 1]);
            
                keys[index] = key;
                ++m_count;
            }

            void remove_key(size_t index)
            {
                if (index >= m_count)
                    return;

                auto* keys = reinterpret_cast<Key*>(m_key_store);
            
                size_t i = index;
                for (; i < m_count - 1; ++i)
                    keys[i] = std::move(keys[i + 1]);
                keys[i].~Key();

                --m_count;
            }

            void resize_keys(size_t size)
            {
                if (size >= m_count)
                    return;

                auto* keys = reinterpret_cast<Key*>(m_key_store);
                for (size_t i = size; i < m_count; ++i)
                    keys[i].~Key();

                m_count = size;
            }

        private:
            alignas(alignof(Key)) std::byte m_key_store[sizeof(Key) * Order];
            size_t m_count = 0;
        };// class Node

        struct SplitInternalResult
        {
            Node* newNode;
            Key separator;
        };

        struct NodeLeaf : public Node
        {
            NodeLeaf* prev = nullptr;
            NodeLeaf* next = nullptr;
            AlignedValueStorage values[Order];

            NodeLeaf() = default;

            NodeLeaf(const NodeLeaf&) = delete;
            NodeLeaf& operator=(const NodeLeaf&) = delete;

            void unlink()
            {
                if (prev)
                {
                    assert(prev->next == this);
                    prev->next = next;
                }

                if (next)
                {
                    assert(next->prev == this);
                    next->prev = prev;
                }

                prev = nullptr;
                next = nullptr;
            }

            void insert_after(NodeLeaf* left)
            {
                assert(prev == nullptr);
                assert(next == nullptr);
                assert(left != nullptr);

                NodeLeaf* right = left->next;

                prev = left;
                left->next = this;

                next = right;
                if (right)
                    right->prev = this;
            }

            // Note: No deallocation is performed!
            NodeLeaf* merge_right()
            {
                auto* left = this;
                auto* right = this->next;

                assert(right != nullptr);

                for (size_t i = 0; i < right->count(); ++i)
                {
                    left->add_key(std::move(right->key(i)));
                    if constexpr (Params.MoveValueFn != nullptr)
                    {
                        Params.MoveValueFn(left->values[left->count() - 1].data, right->values[i].data);
                        Params.DestroyValueFn(right->values[i].data);
                    }
                }

                right->unlink();
                return right;
            }

            NodeLeaf* split(void* mem_block)
            {
                size_t mid = this->count() / 2;
                NodeLeaf* sibling = new (mem_block)NodeLeaf;
                const size_t sibling_count = this->count() - mid;

                for (size_t i = 0; i < sibling_count; ++i)
                {
                    sibling->add_key(std::move(this->key(mid + i)));

                    if constexpr (Params.MoveValueFn != nullptr)
                    {
                        Params.MoveValueFn(sibling->values[i].data, this->values[mid + i].data);
                        Params.DestroyValueFn(this->values[mid + i].data);
                    }
                }
                this->resize_keys(mid);
                sibling->insert_after(this);

                return sibling;
            }


            void* insert(size_t index, const Key& key)
            {
                assert(index <= this->count());
                assert(this->count() < Order);

                // Make room for new value
                if constexpr (Params.MoveValueFn != nullptr)
                {
                    for (size_t j = this->count(); j > index; --j)
                    {
                        Params.MoveValueFn(this->values[j].data, this->values[j - 1].data);
                        Params.DestroyValueFn(this->values[j - 1].data);
                    }
                }

                this->insert_key(index, key);

                return values[index].data;
            }

            void remove(size_t index)
            {
                // compactar
                if constexpr (Params.MoveValueFn != nullptr)
                {
                    Params.DestroyValueFn(this->values[index].data);
                    for (size_t j = index; j + 1 < this->count(); ++j)
                    {
                        Params.MoveValueFn(this->values[j].data, this->values[j + 1].data);
                        Params.DestroyValueFn(this->values[j + 1].data);
                    }
                }

                this->remove_key(index);
            }

            void rotate_left()
            {
                assert(this->prev != nullptr);
                assert(this->count() > 0);
                NodeLeaf* left = this->prev;

                left->add_key(this->key(0));

                if constexpr (Params.MoveValueFn != nullptr)
                    Params.MoveValueFn(left->values[left->count() - 1].data, this->values[0].data);

                this->remove(0);
            }

            void rotate_right()
            {
                assert(this->next != nullptr);
                assert(this->count() > 0);
                NodeLeaf* right = this->next;
                const size_t lastIndex = this->count() - 1;

                void* valuePtr = right->insert(0, this->key(lastIndex));

                if constexpr (Params.MoveValueFn != nullptr)
                    Params.MoveValueFn(valuePtr, this->values[lastIndex].data);

                this->remove(lastIndex);
            }

        };// struct NodeLeaf

        struct NodeInternal : public Node
        {
            Node* children[Order + 1];

            NodeInternal() = default;
            NodeInternal(Node* left)
            {
                children[0] = left;
            }
            NodeInternal(Node* left, Key&& key, Node* right)
            {
                children[0] = left;
                children[1] = right;
                this->add_key(std::move(key));
            }

            void insert_left(size_t index, Node* child, const Key& key)
            {
                this->insert_key(index, key);
                this->insert_child(index, child);
            }

            void insert_right(size_t index, const Key& key, Node* child)
            {
                this->insert_key(index, key);
                this->insert_child(index + 1, child);
            }

            void add(const Key& key, Node* child)
            {
                this->add_key(key);
                this->children[this->count()] = child;
            }

            void remove_left(size_t index)
            {
                this->remove_child(index);
                this->remove_key(index);
            }

            void remove_right(size_t index)
            {
                this->remove_child(index + 1);
                this->remove_key(index);
            }

            SplitInternalResult split(void* mem_block)
            {
                assert(this->children[0] != nullptr);

                size_t mid_index = this->count() / 2;
                NodeInternal* sibling = new (mem_block) NodeInternal(this->children[mid_index + 1]);
                SplitInternalResult result{ sibling, this->key(mid_index) };

                // Move keys. Keys re divided like this:
                // - mid_index keys remain in the original node.
                // - Next 1 (one) key is returned, will be used as separator in parent node.
                // - The rest go into 'sibling' node.
                for (size_t i = mid_index + 1; i < this->count(); ++i)
                    sibling->add(this->key(i), this->children[i + 1]);

                this->resize_keys(mid_index);

                assert(sibling->children[0] != nullptr);
                assert(this->children[0] != nullptr);

                return result;
            }

            void merge(NodeInternal* right, const Key& separator)
            {
                this->add(separator, right->children[0]);
            
                for (size_t i = 0; i < right->count(); ++i)
                    this->add(right->key(i), right->children[i + 1]);
            }

        private:

            void remove_child(size_t index)
            {
                for (size_t i = index; i < this->count(); ++i)
                    children[i] = children[i + 1];
            }

            void insert_child(size_t index, Node* child)
            {
                assert(index <= this->count());
                for (size_t i = this->count(); i > index; --i)
                    children[i] = children[i - 1];

                children[index] = child;
            }
        };

        struct LeafPos
        {
            NodeLeaf* leaf;
            size_t index;
        };

        struct InsertResultInternal : public InsertResult
        {
            std::optional<SplitInternalResult> split;
        };

        Node* m_root;
        IAllocator* m_alloc;
        size_t m_size = 0;
        unsigned m_height;

        template <typename T> void freeNode(T* ptr);
        void createInitialRootIfNeeded();

        InsertResultInternal insert_recursive(Node* node, const Key& key, unsigned level);
        InsertResultInternal insert_at_leaf(NodeLeaf* leaf, const Key& key);
        InsertResultInternal insert_at_internal(NodeInternal* node, const Key& key, unsigned level);

        NodeLeaf* split_leaf(NodeLeaf* leaf);
        SplitInternalResult split_internal(NodeInternal* node);

        void delete_subtree(Node* node, unsigned level);
        NodeLeaf* leftmost_leaf() const;
        NodeLeaf* rightmost_leaf() const;

        bool erase_recursive(Node* node, const Key& key, unsigned level);
        bool erase_from_leaf(NodeLeaf* leaf, const Key& key);
        void fix_underflow(NodeInternal* parent, size_t idx, unsigned level);
        void rotate_left_leaf(NodeInternal* parent, size_t parentIndex);
        void rotate_right_leaf(NodeInternal* parent, size_t parentIndex);
        void rotate_left_internal(NodeInternal* left, NodeInternal* right, NodeInternal* parent, size_t parentIndex);
        void rotate_right_internal(NodeInternal* left, NodeInternal* right, NodeInternal* parent, size_t parentIndex);
        void merge_leaf(NodeInternal* parent, size_t parentIndex);
        void merge_internal(NodeInternal* parent, size_t parentIndex);
    };


    // ------------------------------------------------------------
    // Constructores
    // ------------------------------------------------------------
    template<typename Key, BTreeCoreParams Params>
    BTreeCore<Key, Params>::BTreeCore(IAllocator& alloc)
        : m_alloc(&alloc)
        , m_root(nullptr)
        , m_size(0)
        , m_height(0)
    {}

    // Definición del constructor de movimiento
    template<typename Key, BTreeCoreParams Params>
    BTreeCore<Key, Params>::BTreeCore(BTreeCore&& rhs) noexcept
        : m_root(rhs.m_root)
        , m_alloc(rhs.m_alloc)
        , m_size(rhs.m_size)
        , m_height(rhs.m_height)
    {
        rhs.m_root = nullptr;
        rhs.m_height = 0;
        rhs.m_size = 0;
    }

    // Definición del operador de movimiento
    template<typename Key, BTreeCoreParams Params>
    BTreeCore<Key, Params>& BTreeCore<Key, Params>::operator=(BTreeCore&& rhs)noexcept
    {
        m_root = rhs.m_root;
        m_alloc = rhs.m_alloc;
        m_size = rhs.m_size;
        m_height = rhs.m_height;

        rhs.m_root = nullptr;
        rhs.m_height = 0;
        rhs.m_size = 0;

        return *this;
    }

    // ------------------------------------------------------------
    // Gestión de memoria
    // ------------------------------------------------------------
    template<typename Key, BTreeCoreParams Params>
    template <typename T>
    void BTreeCore<Key, Params>::freeNode(T* ptr)
    {
        if (ptr)
        {
            ptr->~T();
            m_alloc->free(ptr);
        }
    }

    template<typename Key, BTreeCoreParams Params>
    void BTreeCore<Key, Params>::createInitialRootIfNeeded()
    {
        if (m_root != nullptr)
            return;

        assert(m_height == 0);

        void* mem_block = checked_alloc<NodeLeaf>(*m_alloc);
        m_root = new (mem_block) NodeLeaf;
        m_height = 1;
    }

    // ------------------------------------------------------------
    // Inicialización y limpieza
    // ------------------------------------------------------------

    template<typename Key, BTreeCoreParams Params>
    BTreeCore<Key, Params>::~BTreeCore()
    {
        clear();
    }

    template<typename Key, BTreeCoreParams Params>
    void BTreeCore<Key, Params>::clear()
    {
        if (m_root != nullptr)
            delete_subtree(m_root, 0);

        m_root = nullptr;
        m_height = 0;
        m_size = 0;
    }

    template<typename Key, BTreeCoreParams Params>
    void BTreeCore<Key, Params>::delete_subtree(Node* node, unsigned level)
    {
        if (level == m_height - 1)
        {
            NodeLeaf* leaf = static_cast<NodeLeaf*>(node);
        
            if constexpr(Params.DestroyValueFn != nullptr)
            {
                for (size_t i = 0; i < leaf->count(); ++i)
                    Params.DestroyValueFn(leaf->values[i].data);
            }

            freeNode(leaf);
        }
        else
        {
            NodeInternal* internal = static_cast<NodeInternal*>(node);
            for (size_t i = 0; i <= internal->count(); ++i)
                delete_subtree(internal->children[i], level + 1);
            freeNode(internal);
        }
    }

    // ------------------------------------------------------------
    // División de nodos
    // ------------------------------------------------------------
    template<typename Key, BTreeCoreParams Params>
    typename BTreeCore<Key, Params>::NodeLeaf* BTreeCore<Key, Params>::split_leaf(NodeLeaf* leaf)
    {
        return leaf->split(checked_alloc<NodeLeaf>(*m_alloc));
    }

    template<typename Key, BTreeCoreParams Params>
    typename BTreeCore<Key, Params>::SplitInternalResult BTreeCore<Key, Params>::split_internal(NodeInternal* node)
    {
        void* mem_block = checked_alloc<NodeInternal>(*m_alloc);
        return node->split(mem_block);
    }

    // ------------------------------------------------------------
    // Inserción
    // ------------------------------------------------------------
    template<typename Key, BTreeCoreParams Params>
    typename BTreeCore<Key, Params>::InsertResultInternal BTreeCore<Key, Params>::insert_at_leaf(
        NodeLeaf* leaf
        , const Key& key
    )
    {
        size_t i = 0;
        while (i < leaf->count() && leaf->key(i) < key)
            ++i;

        const Handle location(leaf, i);

        if (i < leaf->count() && leaf->key(i) == key)
            return { location, leaf->values[i].data, false, {} };

        if (leaf->count() >= Order)
        {
            NodeLeaf* new_sibling = split_leaf(leaf);
            NodeLeaf* target_leaf = i < leaf->count() ? leaf : new_sibling;
            auto result = insert_at_leaf(target_leaf, key);
            result.split = SplitInternalResult{ new_sibling, new_sibling->key(0) };
            return result;
        }

        void* valuePtr = leaf->insert(i, key);

        return { location, valuePtr, true, {} };
    }

    template<typename Key, BTreeCoreParams Params>
    typename BTreeCore<Key, Params>::InsertResultInternal BTreeCore<Key, Params>::insert_at_internal(
        NodeInternal* node
        , const Key& key
        , unsigned level
    )
    {
        size_t i = 0;
        while (i < node->count() && !(key < node->key(i)))
            ++i;

        auto result = insert_recursive(node->children[i], key, level + 1);
        if (!result.split.has_value())
            return result;

        assert(node->children[0] != nullptr);
        node->insert_right(i, result.split->separator, result.split->newNode);

        if (node->count() < Order)
            result.split = std::nullopt;
        else
            result.split = split_internal(node);

        return result;
    }

    template<typename Key, BTreeCoreParams Params>
    typename BTreeCore<Key, Params>::InsertResultInternal BTreeCore<Key, Params>::insert_recursive(
        Node* node
        , const Key& key
        , unsigned level
    )
    {
        if (level == m_height - 1)
            return insert_at_leaf(static_cast<NodeLeaf*>(node), key);
        else 
            return insert_at_internal(static_cast<NodeInternal*>(node), key, level);
    }

    // ------------------------------------------------------------
    // Inserción pública
    // ------------------------------------------------------------
    template<typename Key, BTreeCoreParams Params>
    typename BTreeCore<Key, Params>::InsertResult
    BTreeCore<Key, Params>::insert(const Key& key)
    {
        createInitialRootIfNeeded();

        auto result = insert_recursive(m_root, key, 0);

        if (result.split.has_value())
        {
            void* mem_block = checked_alloc<NodeInternal>(*m_alloc);
            NodeInternal* new_root = new (mem_block)NodeInternal(
                m_root
                , std::move(result.split->separator)
                , result.split->newNode
            );

            m_root = new_root;
            ++m_height;
        }

        if (result.newEntry)
            ++m_size;

        return result;
    }

    // ------------------------------------------------------------
    // Búsqueda
    // ------------------------------------------------------------
    template<typename Key, BTreeCoreParams Params>
    typename BTreeCore<Key, Params>::Handle
    BTreeCore<Key, Params>::lower_bound(const Key& key) const
    {
        if (m_root == nullptr)
            return {};

        void* node = m_root;
        unsigned level = 0;

        while (level < m_height - 1)
        {
            NodeInternal* internal = static_cast<NodeInternal*>(node);
        
            size_t i = 0;
            while (i < internal->count() && !(key < internal->key(i)))
                ++i;
            
            node = internal->children[i];
            ++level;
        }

        NodeLeaf* leaf = static_cast<NodeLeaf*>(node);
        for (size_t i = 0; i < leaf->count(); ++i)
        {
            if (!(leaf->key(i) < key))
                return Handle(leaf, i);
        }

        return Handle(leaf->next, 0);
    }

    template<typename Key, BTreeCoreParams Params>
    typename BTreeCore<Key, Params>::Range
    BTreeCore<Key, Params>::range(const Key& key) const
    {
        // TODO: Try to make a better, O(log(n)) implementation.
        auto first = find_first(key);
        if (!first)
            return {};

        Range it(first.m_leaf, first.m_index);

        for (; !it.empty() && it.key() == key; ++it)
        {}

        return Range{ first.m_leaf, first.m_index, it.m_leaf, it.m_index };
    }


    template<typename Key, BTreeCoreParams Params>
    size_t BTreeCore<Key, Params>::count(const Key& key) const
    {
        // TODO: We should aim to make an O(log(n)) implementation.
        size_t counter = 0;
    
        for (auto r = range(key); !r.empty(); ++r)
            ++counter;

        return counter;
    }

    // ------------------------------------------------------------
    // Buscar extremos
    // ------------------------------------------------------------
    template<typename Key, BTreeCoreParams Params>
    typename BTreeCore<Key, Params>::NodeLeaf* BTreeCore<Key, Params>::leftmost_leaf() const
    {
        if (m_root == nullptr)
            return nullptr;

        void* node = m_root;
        unsigned level = 0;
        while (level < m_height - 1)
        {
            NodeInternal* internal = static_cast<NodeInternal*>(node);
            node = internal->children[0];
            ++level;
        }
        return static_cast<NodeLeaf*>(node);
    }

    template<typename Key, BTreeCoreParams Params>
    typename BTreeCore<Key, Params>::NodeLeaf* BTreeCore<Key, Params>::rightmost_leaf() const
    {
        if (m_root == nullptr)
            return nullptr;

        void* node = m_root;
        unsigned level = 0;

        // Bajamos en el árbol desde la raíz hasta la hoja nivel a nivel
        // siempre tomando el último hijo de cada nodo interno (indice count)
        while (level < m_height - 1)
        {
            NodeInternal* internal = static_cast<NodeInternal*>(node);
            node = internal->children[internal->count()];
            ++level;
        }

        return static_cast<NodeLeaf*>(node);
    }

    // ------------------------------------------------------------
    // Iteración
    // ------------------------------------------------------------
    template<typename Key, BTreeCoreParams Params>
    typename BTreeCore<Key, Params>::Range
    BTreeCore<Key, Params>::begin() const
    {
        NodeLeaf* leaf = leftmost_leaf();
        if (!leaf || leaf->count() == 0)
            return Range();
        return Range(leaf, 0);
    }

    template<typename Key, BTreeCoreParams Params>
    typename BTreeCore<Key, Params>::InvRange
    BTreeCore<Key, Params>::rbegin() const
    {
        NodeLeaf* leaf = rightmost_leaf();
        if (!leaf || leaf->count() == 0)
            return InvRange();
        return InvRange(leaf, leaf->count() - 1);
    }

    // ------------------------------------------------------------
    // Borrado con redistribución y fusión
    // ------------------------------------------------------------

    template<typename Key, BTreeCoreParams Params>
    bool BTreeCore<Key, Params>::erase(const Key& key)
    {
        if (!m_root)
            return false;

        if (!erase_recursive(m_root, key, 0))
            return false;

        // Si la raíz se quedó sin claves y tiene un solo hijo, lo promovemos.
        if (m_height > 1)
        {
            NodeInternal* rootInternal = static_cast<NodeInternal*>(m_root);
            if (rootInternal->count() == 0) 
            {
                Node* newRoot = rootInternal->children[0];
                m_root = newRoot;
                freeNode(rootInternal);
                --m_height;
            }
        }
        else if (m_height == 1) 
        {
            NodeLeaf* rootLeaf = static_cast<NodeLeaf*>(m_root);
            if (rootLeaf->count() == 0) 
            {
                freeNode(rootLeaf);
                m_root = nullptr;
                m_height = 0;
            }
        }

        --m_size;
        return true;
    }

    // ------------------------------------------------------------
    // Función auxiliar recursiva de borrado
    // ------------------------------------------------------------

    template<typename Key, BTreeCoreParams Params>
    bool BTreeCore<Key, Params>::erase_recursive(Node* node, const Key& key, unsigned level)
    {
        if (level == m_height - 1)
            return erase_from_leaf(static_cast<NodeLeaf*>(node), key);

        NodeInternal* internal = static_cast<NodeInternal*>(node);
        size_t i = 0;
        while (i < internal->count() && !(key < internal->key(i)))
            ++i;

        bool erased = erase_recursive(internal->children[i], key, level + 1);
        if (!erased)
            return false;

        fix_underflow(internal, i, level);
        return true;
    }

    // ------------------------------------------------------------
    // Borrado dentro de una hoja
    // ------------------------------------------------------------

    template<typename Key, BTreeCoreParams Params>
    bool BTreeCore<Key, Params>::erase_from_leaf(NodeLeaf* leaf, const Key& key)
    {
        size_t i = 0;
        while (i < leaf->count() && leaf->key(i) < key)
            ++i;

        if (i == leaf->count() || leaf->key(i) != key)
            return false;

        leaf->remove(i);

        return true;
    }

    // ------------------------------------------------------------
    // Corrección de underflow (redistribución o fusión)
    // ------------------------------------------------------------

    template<typename Key, BTreeCoreParams Params>
    void BTreeCore<Key, Params>::fix_underflow(NodeInternal* parent, size_t idx, unsigned level)
    {
        Node* child = parent->children[idx];
        bool isLeaf = (level == m_height - 2);

        // Si el nodo no está por debajo del mínimo, no hacemos nada
        size_t minKeys = (Order + 1) / 2 - 1;
        if (child->count() >= minKeys)
            return;

        Node* leftSibling = (idx > 0) ? parent->children[idx - 1] : nullptr;
        Node* rightSibling = (idx + 1 <= parent->count()) ? parent->children[idx + 1] : nullptr;

        // Intentar redistribuir
        if (leftSibling && leftSibling->count() > minKeys)
        {
            if (isLeaf)
                rotate_right_leaf(parent, idx - 1);
            else
            {
                rotate_right_internal(
                    static_cast<NodeInternal*>(leftSibling)
                    , static_cast<NodeInternal*>(child)
                    , parent
                    , idx - 1
                );
            }
        }
        else if (rightSibling && rightSibling->count() > minKeys)
        {
            if (isLeaf)
                rotate_left_leaf(parent, idx);
            else
            {
                rotate_left_internal(
                    static_cast<NodeInternal*>(child)
                    , static_cast<NodeInternal*>(rightSibling)
                    , parent
                    , idx
                );
            }        
        }    
        else 
        {
            // Si no se puede redistribuir, fusionar
            if (leftSibling) 
            {
                if (isLeaf)
                    merge_leaf(parent, idx - 1);
                else
                    merge_internal(parent, idx - 1);
            }
            else if (rightSibling)
            {
                if (isLeaf)
                    merge_leaf(parent, idx);
                else
                    merge_internal(parent, idx);
            }
        }
    }

    // ------------------------------------------------------------
    // Redistribución entre hojas
    // ------------------------------------------------------------

    template<typename Key, BTreeCoreParams Params>
    void BTreeCore<Key, Params>::rotate_left_leaf(NodeInternal* parent, size_t parentIndex)
    {
        NodeLeaf* right = static_cast<NodeLeaf*>(parent->children[parentIndex + 1]);
        right->rotate_left();
        parent->change_key(parentIndex, right->key(0));
    }

    template<typename Key, BTreeCoreParams Params>
    void BTreeCore<Key, Params>::rotate_right_leaf(NodeInternal* parent, size_t parentIndex)
    {
        NodeLeaf* left = static_cast<NodeLeaf*>(parent->children[parentIndex]);
        NodeLeaf* right = static_cast<NodeLeaf*>(parent->children[parentIndex + 1]);
        left->rotate_right();
        parent->change_key(parentIndex, right->key(0));
    }

    // ------------------------------------------------------------
    // Redistribución entre nodos internos
    // ------------------------------------------------------------

    template<typename Key, BTreeCoreParams Params>
    void BTreeCore<Key, Params>::rotate_left_internal(NodeInternal* left, NodeInternal* right, NodeInternal* parent, size_t parentIndex)
    {
        assert(right->count() > 1);

        left->add(parent->key(parentIndex), right->children[0]);
        parent->change_key(parentIndex, right->key(0));
        right->remove_left(0);
    }

    template<typename Key, BTreeCoreParams Params>
    void BTreeCore<Key, Params>::rotate_right_internal(NodeInternal* left, NodeInternal* right, NodeInternal* parent, size_t parentIndex)
    {
        assert(left->count() > 1);

        right->insert_left(0, left->children[left->count()], parent->key(parentIndex));
        parent->change_key(parentIndex, left->key(left->count() - 1));
        left->remove_right(left->count() - 1);
    }

    // ------------------------------------------------------------
    // Fusión de nodos
    // ------------------------------------------------------------

    template<typename Key, BTreeCoreParams Params>
    void BTreeCore<Key, Params>::merge_leaf(NodeInternal* parent, size_t parentIndex)
    {
        auto* left = static_cast<NodeLeaf*> (parent->children[parentIndex]);
    
        auto* right = left->merge_right();
        freeNode(right);

        parent->remove_right(parentIndex);
    }

    template<typename Key, BTreeCoreParams Params>
    void BTreeCore<Key, Params>::merge_internal(NodeInternal* parent, size_t parentIndex)
    {
        auto* left = static_cast<NodeInternal*> (parent->children[parentIndex]);
        auto* right = static_cast<NodeInternal*> (parent->children[parentIndex+1]);

        left->merge(right, parent->key(parentIndex));
        parent->remove_right(parentIndex);

        freeNode(right);
    }
} //namespace coll