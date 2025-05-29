#include <GL/glew.h>
#include "modelLoader.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"
#include "lodepng.h"
#include <iostream>
#include <sstream>


bool loadTexture(const std::string& filename, GLuint& textureID) {
    std::vector<unsigned char> image;
    unsigned width, height;
    if (lodepng::decode(image, width, height, filename)) {
        std::cerr << "Failed to load texture: " << filename << std::endl;
        return false;
    }

    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height,
        0, GL_RGBA, GL_UNSIGNED_BYTE, &image[0]);
    glGenerateMipmap(GL_TEXTURE_2D);

    return true;
}

bool loadModel(const std::string& path, const std::string& mtlBasePath, Model& model) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    std::string baseDir = mtlBasePath;

    bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err,
        path.c_str(), baseDir.c_str());

    if (!warn.empty()) std::cout << "WARN: " << warn << std::endl;
    if (!err.empty()) std::cerr << "ERR: " << err << std::endl;
    if (!ret) return false;

    std::vector<float> vertices, normals, texCoords;
    std::vector<unsigned int> indices;

    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            vertices.push_back(attrib.vertices[3 * index.vertex_index + 0]);
            vertices.push_back(attrib.vertices[3 * index.vertex_index + 1]);
            vertices.push_back(attrib.vertices[3 * index.vertex_index + 2]);

            if (index.normal_index >= 0) {
                normals.push_back(attrib.normals[3 * index.normal_index + 0]);
                normals.push_back(attrib.normals[3 * index.normal_index + 1]);
                normals.push_back(attrib.normals[3 * index.normal_index + 2]);
            }
            else {
                normals.push_back(0.0f); normals.push_back(0.0f); normals.push_back(1.0f);
            }

            if (index.texcoord_index >= 0) {
                texCoords.push_back(attrib.texcoords[2 * index.texcoord_index + 0]);
                texCoords.push_back(1.0f - attrib.texcoords[2 * index.texcoord_index + 1]); // Flip V
            }
            else {
                texCoords.push_back(0.0f); texCoords.push_back(0.0f);
            }

            indices.push_back((unsigned int)(indices.size()));
        }
    }

    model.vertices = vertices;
    model.normals = normals;
    model.texCoords = texCoords;
    model.indices = indices;

    glGenVertexArrays(1, &model.vao);
    glGenBuffers(3, model.vbo);
    glGenBuffers(1, &model.ebo);

    glBindVertexArray(model.vao);

    glBindBuffer(GL_ARRAY_BUFFER, model.vbo[0]);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), &vertices[0], GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0); // location = 0
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, model.vbo[1]);
    glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(float), &normals[0], GL_STATIC_DRAW);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, (void*)0); // location = 2
    glEnableVertexAttribArray(2);

    glBindBuffer(GL_ARRAY_BUFFER, model.vbo[2]);
    glBufferData(GL_ARRAY_BUFFER, texCoords.size() * sizeof(float), &texCoords[0], GL_STATIC_DRAW);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void*)0); // location = 1
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, model.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

    glBindVertexArray(0);

    // Wczytaj pierwsza teksture z materialu
    if (!materials.empty() && !materials[0].diffuse_texname.empty()) {
        std::string texPath = baseDir + materials[0].diffuse_texname;
        loadTexture(texPath, model.textureID);
    }

    return true;
}
