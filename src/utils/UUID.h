#pragma once

#include <cstdint>
#include <functional>
#include <string>

class UUID {
public:
    UUID();
    UUID(uint32_t a, uint32_t b, uint32_t c, uint32_t d);

    static UUID random();

    bool operator==(const UUID &other) const;
    bool operator!=(const UUID &other) const;

    const uint32_t *getData() const;
    std::string toString() const;

private:
    uint32_t m_data[4];
};

namespace std {
    template<>
    struct hash<UUID> {
        size_t operator()(const UUID &uuid) const noexcept {
            const uint32_t *d = uuid.getData();
            size_t h          = d[0];
            h ^= (size_t) d[1] << 1;
            h ^= (size_t) d[2] << 2;
            h ^= (size_t) d[3] << 3;
            return h;
        }
    };
} // namespace std
