#include "shaderprogram.h"
#include <cstdio>
#include <cstdlib>
#include <glm/gtc/type_ptr.hpp>

// Read entire file into a null-terminated char*
char* ShaderProgram::readFile(const char* fileName) {
    FILE* file = nullptr;
#pragma warning(suppress:4996)
    file = fopen(fileName, "rb");
    if (!file) return nullptr;

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* buffer = new char[size + 1];
#pragma warning(suppress:6386)
    fread(buffer, 1, size, file);
    buffer[size] = '\0';
    fclose(file);
    return buffer;
}

// Compile a single shader stage, return its GLuint
GLuint ShaderProgram::loadShader(GLenum shaderType, const char* fileName) {
    GLuint shader = glCreateShader(shaderType);
    const char* src = readFile(fileName);
    if (!src) {
        fprintf(stderr, "Failed to load shader file: %s\n", fileName);
        return 0;
    }
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    delete[] src;

    // Check compile log
    GLint logLen = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);
    if (logLen > 1) {
        char* infoLog = new char[logLen];
        GLint written = 0;
        glGetShaderInfoLog(shader, logLen, &written, infoLog);
        printf("Shader compile log (%s):\n%s\n", fileName, infoLog);
        delete[] infoLog;
    }
    return shader;
}

// Build program from VS, optional GS, and FS
ShaderProgram::ShaderProgram(const char* vertexShaderFile,
    const char* geometryShaderFile,
    const char* fragmentShaderFile)
{
    printf("Loading vertex shader: %s\n", vertexShaderFile);
    vertexShader = loadShader(GL_VERTEX_SHADER, vertexShaderFile);

    if (geometryShaderFile) {
        printf("Loading geometry shader: %s\n", geometryShaderFile);
        geometryShader = loadShader(GL_GEOMETRY_SHADER, geometryShaderFile);
    }
    else {
        geometryShader = 0;
    }

    printf("Loading fragment shader: %s\n", fragmentShaderFile);
    fragmentShader = loadShader(GL_FRAGMENT_SHADER, fragmentShaderFile);

    // Link
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    if (geometryShader) glAttachShader(shaderProgram, geometryShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    // Check link log
    GLint logLen = 0;
    glGetProgramiv(shaderProgram, GL_INFO_LOG_LENGTH, &logLen);
    if (logLen > 1) {
        char* infoLog = new char[logLen];
        GLint written = 0;
        glGetProgramInfoLog(shaderProgram, logLen, &written, infoLog);
        printf("Program link log:\n%s\n", infoLog);
        delete[] infoLog;
    }
    printf("Shader program created\n");
}

// Destructor: detach & delete shaders, delete program
ShaderProgram::~ShaderProgram() {
    glDetachShader(shaderProgram, vertexShader);
    if (geometryShader) glDetachShader(shaderProgram, geometryShader);
    glDetachShader(shaderProgram, fragmentShader);

    glDeleteShader(vertexShader);
    if (geometryShader) glDeleteShader(geometryShader);
    glDeleteShader(fragmentShader);

    glDeleteProgram(shaderProgram);
}

// Activate this shader program
void ShaderProgram::use() {
    glUseProgram(shaderProgram);
}

// Get uniform/attribute locations
GLuint ShaderProgram::u(const char* name) {
    return glGetUniformLocation(shaderProgram, name);
}
GLuint ShaderProgram::a(const char* name) {
    return glGetAttribLocation(shaderProgram, name);
}

// Convenience uniform setters

void ShaderProgram::setInt(const char* name, int value) {
    glUniform1i(u(name), value);
}

void ShaderProgram::setFloat(const char* name, float value) {
    glUniform1f(u(name), value);
}

void ShaderProgram::setVec3(const char* name, const glm::vec3& v) {
    glUniform3fv(u(name), 1, glm::value_ptr(v));
}

void ShaderProgram::setMat4(const char* name, const glm::mat4& m) {
    glUniformMatrix4fv(u(name), 1, GL_FALSE, glm::value_ptr(m));
}

void ShaderProgram::setMat3(const char* name, const glm::mat3& m) {
    glUniformMatrix3fv(u(name), 1, GL_FALSE, glm::value_ptr(m));
}
