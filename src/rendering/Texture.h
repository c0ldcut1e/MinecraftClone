#pragma once

#include <cstdint>
#include <vector>

class Texture {
public:
    explicit Texture(const char *path);
    ~Texture();

    void bind(uint32_t slot) const;

    void samplePixel(int pixelX, int pixelY, float &r, float &g, float &b) const;

    int getPixelWidth() const;
    int getPixelHeight() const;

private:
    uint32_t m_id;
    std::vector<uint8_t> m_pixels;
    int m_pixelWidth;
    int m_pixelHeight;
};
