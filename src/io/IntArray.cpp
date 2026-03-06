#include "IntArray.h"

IntArray::IntArray() : m_values() {}

IntArray::IntArray(size_t size) : m_values(size) {}

int32_t IntArray::get(size_t index) const { return m_values[index]; }

void IntArray::set(size_t index, int32_t value) { m_values[index] = value; }

void IntArray::push(int32_t value) { m_values.push_back(value); }

size_t IntArray::size() const { return m_values.size(); }

const std::vector<int32_t> &IntArray::values() const { return m_values; }
