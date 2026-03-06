#include "ByteTag.h"

ByteTag::ByteTag() : m_value(0) {}

ByteTag::ByteTag(int8_t value) : m_value(value) {}

Tag::Type ByteTag::getType() const { return Type::BYTE; }

int8_t ByteTag::getValue() const { return m_value; }

void ByteTag::setValue(int8_t value) { m_value = value; }
