#include "ByteArrayTag.h"

ByteArrayTag::ByteArrayTag() : m_value() {}

ByteArrayTag::ByteArrayTag(const std::vector<uint8_t> &value) : m_value(value) {}

Tag::Type ByteArrayTag::getType() const { return Type::BYTE_ARRAY; }

const std::vector<uint8_t> &ByteArrayTag::getValue() const { return m_value; }

void ByteArrayTag::setValue(const std::vector<uint8_t> &value) { m_value = value; }
