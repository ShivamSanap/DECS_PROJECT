#pragma once

#include <iostream>
#include <string>
#include <list>
#include <unordered_map>
#include <mutex>
#include <optional>

struct CacheState {
    size_t current_size;
    size_t max_size;
    std::list<std::pair<std::string, std::string>> items; 
};


class LRUCache {
public:
    LRUCache(size_t max_size) : m_max_size(max_size) {}

    void put(const std::string& key, const std::string& value) {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto it = m_cache_map.find(key);
        if (it != m_cache_map.end()) {
            m_lru_list.splice(m_lru_list.begin(), m_lru_list, it->second);
            it->second->second = value;
            return;
        }

        if (m_cache_map.size() >= m_max_size) {
            std::string lru_key = m_lru_list.back().first;
            m_cache_map.erase(lru_key);
            m_lru_list.pop_back();
        }

        m_lru_list.push_front({key, value});
        m_cache_map[key] = m_lru_list.begin();
    }

    std::optional<std::string> get(const std::string& key) {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto it = m_cache_map.find(key);
        if (it == m_cache_map.end()) {
            return std::nullopt;
        }

        m_lru_list.splice(m_lru_list.begin(), m_lru_list, it->second);

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

    CacheState get_state() {
        std::lock_guard<std::mutex> lock(m_mutex);
        CacheState state;
        state.current_size = m_cache_map.size();
        state.max_size = m_max_size;
        state.items = m_lru_list;
        return state;
    }


private:
    size_t m_max_size;
    std::list<std::pair<std::string, std::string>> m_lru_list;
    std::unordered_map<std::string, decltype(m_lru_list.begin())> m_cache_map;
    std::mutex m_mutex;
};