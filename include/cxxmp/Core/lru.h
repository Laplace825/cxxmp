#pragma once

#include "cxxmp/Common/typing.h"

#include <fmt/core.h>
#include <map>
#include <memory>

namespace cxxmp {

template < typing::Hashable _Key, typename _Val >
class LRU {
  public:
    struct Node {
        ::std::shared_ptr< Node > next{nullptr};
        ::std::shared_ptr< Node > prev{nullptr};
        Node() = default;

        explicit Node(const _Key& k, const _Val& v) : m_key{k}, m_val{v} {
            next = nullptr;
            prev = nullptr;
        }

        _Key m_key;
        _Val m_val;
    };

    LRU() = delete;

    explicit LRU(size_t size) : m_capacity{size} {
        m_head       = ::std::make_shared< Node >();
        m_tail       = ::std::make_shared< Node >();
        m_head->next = m_tail;
        // Initialize the LRU cache
        m_tail->prev = m_head;
        m_kvCache.clear();
    }

    ~LRU() {
        // Clear the LRU cache
        m_kvCache.clear();
    }

    /**
     * @brief: Get a value from the LRU cache
     * If the value is not found, return false
     * If the value is found, move the node to m_head->next
     *
     */
    typing::Result< _Val > get(const _Key& key) noexcept {
        auto it = this->m_kvCache.find(key);
        if (it == this->m_kvCache.end()) {
            return {false, _Val{}};
        }
        std::shared_ptr< Node > node = it->second;
        // Move the node to the head of the list
        node2head(node);
        return {true, node->m_val};
    }

    /**
     * @brief: Put a value into the LRU cache
     * If the value is already in the cache, update it and move it to the head
     * If the value is not in the cache, add it to the head
     * If the cache is full, remove the last node and add the new node to the
     * head
     */
    void put(const _Key& key, const _Val& val) noexcept {
        if (!this->m_kvCache.count(key)) {
            // do not contain the key inside
            auto newNode = std::make_shared< Node >(key, val);
            m_kvCache.insert_or_assign(key, newNode);
            // add the new node to the head
            node2head(newNode);
            if (m_capacity < m_kvCache.size()) {
                // remove the last node
                auto lastNode = m_tail->prev;
                this->removeNode(lastNode);
                m_kvCache.erase(lastNode->m_key);
            }
        }
        else {
            // contain the key inside
            auto node   = m_kvCache[key];
            node->m_val = val;
            this->removeNode(node);
            node2head(node);
        }
    }

  private:
    // we get the m_head->next every times
    void node2Tail(std::shared_ptr< Node >& node) noexcept {
        removeNode(node);
        node->next         = m_tail;
        node->prev         = m_tail->prev;
        m_tail->prev->next = node;
        m_tail->prev       = node;
    }

    void node2head(std::shared_ptr< Node >& node) noexcept {
        removeNode(node);
        node->next         = m_head->next;
        node->prev         = m_head;
        m_head->next->prev = node;
        m_head->next       = node;
    }

    void removeNode(std::shared_ptr< Node >& node) noexcept {
        if (node == m_head || node == m_tail) {
            return;
        }
        if (node->prev) {
            node->prev->next = node->next;
        }
        if (node->next) {
            node->next->prev = node->prev;
        }
        node->next = nullptr;
        node->prev = nullptr;
    }

  private:
    ::std::map< _Key, ::std::shared_ptr< Node > > m_kvCache;
    ::std::shared_ptr< Node > m_head;
    ::std::shared_ptr< Node > m_tail;

    size_t m_capacity{0};
};

} // namespace cxxmp
