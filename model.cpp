#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#include "model.h"
#include "lodepng.h"
#include <iostream>

Model::Model(const std::string& path) {
    loadModel(path);
    setupMesh();
}

void Model::Draw(ShaderProgram* shader) {
    shader->use();

    glActiveTexture(GL_TEXTURE0); // activate texture unit 0
    glBindTexture(GL_TEXTURE_2D, textureID); // bind the texture
    glUniform1i(shader->u("texture0"), 0); // tell the shader to use it

    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void Model::loadModel(const std::string& path) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    std::string mtlBaseDir = path.substr(0, path.find_last_of("/\\") + 1);

    bool ret = tinyobj::LoadObj(
        &attrib, &shapes, &materials, &warn, &err, path.c_str(), mtlBaseDir.c_str(), true);

    if (!warn.empty()) std::cout << "WARN: " << warn << std::endl;
    if (!err.empty()) std::cerr << "ERR: " << err << std::endl;
    if (!ret) throw std::runtime_error("Failed to load model: " + path);

    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            Vertex vertex{};
            vertex.Position = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            if (!attrib.normals.empty() && index.normal_index >= 0) {
                vertex.Normal = {
                    attrib.normals[3 * index.normal_index + 0],
                    attrib.normals[3 * index.normal_index + 1],
                    attrib.normals[3 * index.normal_index + 2]
                };
            }
            else {
                vertex.Normal = glm::vec3(0.0f); 
            }


            if (!attrib.texcoords.empty() && index.texcoord_index >= 0) {
                vertex.TexCoords = {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
                };
            }
            else {
                vertex.TexCoords = glm::vec2(0.0f);
            }


            vertices.push_back(vertex);
            indices.push_back(indices.size());
        }
    }

    if (!materials.empty()) {
        bool foundTexture = false;

        for (const auto& mat : materials) {
            std::string texFile = mat.diffuse_texname;
            std::cout << "[DEBUG] diffuse_texname: '" << texFile << "'\n";

            if (!texFile.empty()) {
                std::string fullPath = mtlBaseDir + texFile;
                std::cout << "[MODEL] Loading texture from: " << fullPath << std::endl;
                loadTexture(fullPath);
                foundTexture = true;
                break; // first non empty texture
            }
        }

        if (!foundTexture) {
            std::cerr << "[WARNING] No valid texture file found in material file.\n";
        }
    }
    else {
        std::cout << "No material file found or material list is empty.\n";
    }
}

void Model::loadTexture(const std::string& filename) {
    std::vector<unsigned char> image;
    unsigned width, height;
    unsigned error = lodepng::decode(image, width, height, filename);

    if (error) {
        std::cerr << "Failed to load texture " << filename << ": " << lodepng_error_text(error) << "\n";
        return;
    }

    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image.data());

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glGenerateMipmap(GL_TEXTURE_2D);
}

void Model::setupMesh() {
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0); // Position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);

    glEnableVertexAttribArray(1); // Normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Normal));

    glEnableVertexAttribArray(2); // TexCoords
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, TexCoords));

    glBindVertexArray(0);
}
