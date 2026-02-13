#pragma once

#include <cstdint>
#include <unordered_map>

#include "../core/Logger.h"

template<typename K, typename V>
class TypedRegistry {
public:
    void registerValue(K key, V value) { m_values[key] = value; }

    V getValue(K key) const {
        auto it = m_values.find(key);
        if (it == m_values.end()) return nullptr;
        return it->second;
    }

    bool hasValue(K key) const { return m_values.find(key) != m_values.end(); }

private:
    std::unordered_map<K, V> m_values;
};
