#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

class IntArray
{
public:
    IntArray();
    explicit IntArray(size_t size);

    int32_t get(size_t index) const;
    void set(size_t index, int32_t value);
    void push(int32_t value);

    size_t size() const;

    const std::vector<int32_t> &values() const;

private:
    std::vector<int32_t> m_values;
};
