#pragma once

#include <string>
#include <vector>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include "shaderprogram.h"

struct Vertex {
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoords;
};

class Model {
public:
    Model(const std::string& path);
    void Draw(ShaderProgram* shader);

private:
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    GLuint textureID;
    GLuint VAO, VBO, EBO;

    void loadModel(const std::string& path);
    void processMesh();
    void loadTexture(const std::string& filename);
    void setupMesh();
};
