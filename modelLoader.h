#pragma once
#include <string>
#include <vector>
#include <glm/glm.hpp>

struct Model {
    std::vector<float> vertices;
    std::vector<float> normals;
    std::vector<float> texCoords;
    std::vector<unsigned int> indices;
    GLuint vao, vbo[3], ebo;
    GLuint textureID;
};

bool loadModel(const std::string& path, const std::string& mtlBasePath, Model& model);
