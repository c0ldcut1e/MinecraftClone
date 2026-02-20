#include "Shader.h"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>

#include <glad/glad.h>

#include "RenderCommand.h"

Shader::Shader(const char *vertexPath, const char *fragmentPath) {
    char *vertexSource   = loadFile(vertexPath);
    char *fragmentSource = loadFile(fragmentPath);

    uint32_t vertex   = compileShader(GL_VERTEX_SHADER, vertexSource);
    uint32_t fragment = compileShader(GL_FRAGMENT_SHADER, fragmentSource);

    free(vertexSource);
    free(fragmentSource);

    m_program = RenderCommand::createProgram();
    RenderCommand::attachShader(m_program, vertex);
    RenderCommand::attachShader(m_program, fragment);
    RenderCommand::linkProgram(m_program);

    if (!RenderCommand::getProgramLinkStatus(m_program)) throw std::runtime_error("program link failed");

    RenderCommand::deleteShader(vertex);
    RenderCommand::deleteShader(fragment);
}

Shader::~Shader() { RenderCommand::deleteProgram(m_program); }

void Shader::bind() const { RenderCommand::useProgram(m_program); }

int Shader::getLocation(const char *name) const {
    auto it = m_locationCache.find(name);
    if (it != m_locationCache.end()) return it->second;
    int location          = RenderCommand::getUniformLocation(m_program, name);
    m_locationCache[name] = location;
    return location;
}

void Shader::setInt(const char *name, int value) const { RenderCommand::setUniform1i(getLocation(name), value); }

void Shader::setFloat(const char *name, float value) const { RenderCommand::setUniform1f(getLocation(name), value); }

void Shader::setVec2(const char *name, float x, float y) const { RenderCommand::setUniform2f(getLocation(name), x, y); }

void Shader::setVec3(const char *name, float x, float y, float z) const { RenderCommand::setUniform3f(getLocation(name), x, y, z); }

void Shader::setMat4(const char *name, const double *data) const {
    float matrix[16];
    for (int i = 0; i < 16; i++) matrix[i] = (float) data[i];
    RenderCommand::setUniformMatrix4fv(getLocation(name), 1, false, matrix);
}

char *Shader::loadFile(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) throw std::runtime_error("failed to open shader");

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);

    char *buffer = (char *) malloc(size + 1);
    fread(buffer, 1, size, file);
    buffer[size] = 0;

    fclose(file);
    return buffer;
}

uint32_t Shader::compileShader(uint32_t type, const char *src) {
    uint32_t shader = RenderCommand::createShader(type);

    RenderCommand::shaderSource(shader, src);
    RenderCommand::compileShader(shader);

    if (!RenderCommand::getShaderCompileStatus(shader)) throw std::runtime_error("shader compile failed");

    return shader;
}
