#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "../core/Logger.h"

template<typename T>
class MappedRegistry {
public:
    uint32_t registerValue(const std::string &name, const T &value) {
        uint32_t id      = (uint32_t) m_values.size();
        m_nameToId[name] = id;
        m_values.push_back(value);
        return id;
    }

    T *byId(uint32_t id) {
        if (id >= (uint32_t) m_values.size()) return nullptr;
        return &m_values[id];
    }

    const T *byId(uint32_t id) const {
        if (id >= (uint32_t) m_values.size()) return nullptr;
        return &m_values[id];
    }

    uint32_t idOf(const T *value) const {
        for (uint32_t i = 0; i < m_values.size(); i++)
            if (&m_values[i] == value) return i;

        return 0;
    }

    uint32_t getId(const std::string &name) const { return m_nameToId.at(name); }

    bool has(const std::string &name) const { return m_nameToId.find(name) != m_nameToId.end(); }

    uint32_t size() const { return (uint32_t) m_values.size(); }

private:
    std::unordered_map<std::string, uint32_t> m_nameToId;
    std::vector<T> m_values;
};
