#ifndef MESH_H
#define MESH_H

#include <GL/glew.h>
#include <glm/glm.hpp>

#include <string>
#include <vector>

#include "shader.h"

struct Vertex {
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoords;
    glm::ivec4 BoneIDs = glm::ivec4(0);
    glm::vec4 Weights = glm::vec4(0.0f);
};

struct Texture {
    unsigned int id;
    std::string type;
    std::string path;
};

inline unsigned int WhiteTexture() {
    static unsigned int id = 0;
    if (id == 0) {
        const unsigned char white[4] = {255, 255, 255, 255};
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    return id;
}

class Mesh {
public:
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    std::vector<Texture> textures;
    glm::vec4 baseColor{1.0f};
    unsigned int VAO;

    Mesh(std::vector<Vertex> vertices, std::vector<unsigned int> indices, std::vector<Texture> textures,
         const glm::vec4 &baseColor = glm::vec4(1.0f)) {
        this->vertices = vertices;
        this->indices = indices;
        this->textures = textures;
        this->baseColor = baseColor;

        setupMesh();
    }

    void Draw(Shader &shader) {
        unsigned int diffuseNr = 1;
        unsigned int specularNr = 1;

        for (unsigned int i = 0; i < textures.size(); i++) {
            glActiveTexture(GL_TEXTURE0 + i);

            std::string number;
            std::string name = textures[i].type;
            if (name == "texture_diffuse")
                number = std::to_string(diffuseNr++);
            else if (name == "texture_specular")
                number = std::to_string(specularNr++);

            shader.setInt(name + number, i);
            glBindTexture(GL_TEXTURE_2D, textures[i].id);
        }

        if (diffuseNr == 1) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, WhiteTexture());
            shader.setInt("texture_diffuse1", 0);
            shader.setVec4("baseColor", baseColor);
        } else {
            shader.setVec4("baseColor", glm::vec4(1.0f));
        }

        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, static_cast<unsigned int>(indices.size()), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        glActiveTexture(GL_TEXTURE0);
    }

private:
    unsigned int VBO, EBO;

    void setupMesh() {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)0);

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, Normal));

        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, TexCoords));

        glEnableVertexAttribArray(3);
        glVertexAttribIPointer(3, 4, GL_INT, sizeof(Vertex), (void *)offsetof(Vertex, BoneIDs));

        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, Weights));

        glBindVertexArray(0);
    }
};

#endif
