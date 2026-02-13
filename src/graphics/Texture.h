#pragma once

#include <cstdint>

class Texture {
public:
    explicit Texture(const char *path);
    ~Texture();

    void bind(uint32_t slot) const;

private:
    uint32_t m_id;
};
