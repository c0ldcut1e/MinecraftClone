#include "ByteArrayOutputStream.h"

ByteArrayOutputStream::ByteArrayOutputStream() : m_data() {}

void ByteArrayOutputStream::write(uint8_t value) { m_data.push_back(value); }

const std::vector<uint8_t> &ByteArrayOutputStream::toByteArray() const { return m_data; }

void ByteArrayOutputStream::clear() { m_data.clear(); }
