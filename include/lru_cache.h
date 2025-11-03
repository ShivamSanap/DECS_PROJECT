#pragma once

#include <iostream>
#include <string>
#include <list>
#include <unordered_map>
#include <mutex>
#include <optional>

// ++ ADD THIS STRUCT ++
/**
 * @brief A snapshot of the cache's current state.
 */
struct CacheState {
    size_t current_size;
    size_t max_size;
    // A copy of the items, from Most-Recently-Used to Least-Recently-Used
    std::list<std::pair<std::string, std::string>> items; 
};


class LRUCache {
public:
    LRUCache(size_t max_size) : m_max_size(max_size) {}

    // ... (keep the existing 'put', 'get', and 'remove' methods as they are) ...

    void put(const std::string& key, const std::string& value) {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Check if key already exists
        auto it = m_cache_map.find(key);
        if (it != m_cache_map.end()) {
            // Key exists, move to front (most recently used)
            m_lru_list.splice(m_lru_list.begin(), m_lru_list, it->second);
            // Update the value
            it->second->second = value;
            return;
        }

        // Key doesn't exist, check if cache is full
        if (m_cache_map.size() >= m_max_size) {
            // Evict the least recently used item (at the back of the list)
            std::string lru_key = m_lru_list.back().first;
            m_cache_map.erase(lru_key);
            m_lru_list.pop_back();
        }

        // Insert the new key-value pair at the front
        m_lru_list.push_front({key, value});
        m_cache_map[key] = m_lru_list.begin();
    }

    std::optional<std::string> get(const std::string& key) {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto it = m_cache_map.find(key);
        if (it == m_cache_map.end()) {
            // Not found
            return std::nullopt;
        }

        // Found, move to front
        m_lru_list.splice(m_lru_list.begin(), m_lru_list, it->second);
        // Return the value
        return it->second->second;
    }

    void remove(const std::string& key) {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto it = m_cache_map.find(key);
        if (it != m_cache_map.end()) {
            m_lru_list.erase(it->second);
            m_cache_map.erase(it);
        }
    }

    // ++ ADD THIS NEW METHOD ++
    /**
     * @brief Gets a snapshot of the current cache state.
     * This is thread-safe.
     */
    CacheState get_state() {
        std::lock_guard<std::mutex> lock(m_mutex);
        CacheState state;
        state.current_size = m_cache_map.size();
        state.max_size = m_max_size;
        state.items = m_lru_list; // This creates a copy of the list
        return state;
    }


private:
    size_t m_max_size;
    // List of (key, value) pairs. Front is MRU, Back is LRU.
    std::list<std::pair<std::string, std::string>> m_lru_list;
    // Map from key to an iterator in the list
    std::unordered_map<std::string, decltype(m_lru_list.begin())> m_cache_map;
    // Mutex for thread safety
    std::mutex m_mutex;
};