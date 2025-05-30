#ifndef SHADERPROGRAM_H
#define SHADERPROGRAM_H

#include <GL/glew.h>
#include <glm/glm.hpp>

class ShaderProgram {
private:
    GLuint shaderProgram;   // program handle
    GLuint vertexShader;    // vertex shader handle
    GLuint geometryShader;  // geometry shader handle
    GLuint fragmentShader;  // fragment shader handle

    // load text file into null-terminated char*
    char* readFile(const char* fileName);
    // compile one shader stage and return its handle
    GLuint loadShader(GLenum shaderType, const char* fileName);

public:
    // build from VS, optional GS, and FS
    ShaderProgram(const char* vertexShaderFile,
        const char* geometryShaderFile,
        const char* fragmentShaderFile);
    ~ShaderProgram();

    // use this shader program
    void use();

    // get locations
    GLuint u(const char* variableName); // uniform
    GLuint a(const char* variableName); // attribute

    // --- new convenience setters ---
    void setInt(const char* name, int value);
    void setFloat(const char* name, float value);
    void setVec3(const char* name, const glm::vec3& v);
    void setMat4(const char* name, const glm::mat4& m);
    void setMat3(const char* name, const glm::mat3& m);
};

#endif // SHADERPROGRAM_H
