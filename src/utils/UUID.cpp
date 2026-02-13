#include "UUID.h"
#include "../utils/Random.h"

static Random s_uuidRng;

UUID::UUID() : m_data{0, 0, 0, 0} {}

UUID::UUID(uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    m_data[0] = a;
    m_data[1] = b;
    m_data[2] = c;
    m_data[3] = d;
}

UUID UUID::random() { return UUID(s_uuidRng.nextUInt(), s_uuidRng.nextUInt(), s_uuidRng.nextUInt(), s_uuidRng.nextUInt()); }

bool UUID::operator==(const UUID &other) const { return m_data[0] == other.m_data[0] && m_data[1] == other.m_data[1] && m_data[2] == other.m_data[2] && m_data[3] == other.m_data[3]; }

bool UUID::operator!=(const UUID &other) const { return !(*this == other); }

const uint32_t *UUID::getData() const { return m_data; }

std::string UUID::toString() const {
    char buffer[37];
    snprintf(buffer, sizeof(buffer), "%08x-%08x-%08x-%08x", m_data[0], m_data[1], m_data[2], m_data[3]);
    return std::string(buffer);
}
