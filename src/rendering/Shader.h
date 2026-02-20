#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

class Shader {
public:
    Shader(const char *vertexPath, const char *fragmentPath);
    ~Shader();

    void bind() const;

    void setInt(const char *name, int value) const;
    void setFloat(const char *name, float value) const;
    void setVec2(const char *name, float x, float y) const;
    void setVec3(const char *name, float x, float y, float z) const;
    void setMat4(const char *name, const double *data) const;

private:
    int getLocation(const char *name) const;

    uint32_t m_program;
    mutable std::unordered_map<std::string, int> m_locationCache;

    char *loadFile(const char *path);
    uint32_t compileShader(uint32_t type, const char *source);
};
