#ifndef MODEL_H
#define MODEL_H

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <stb_image.h>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <cfloat>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "mesh.h"
#include "shader.h"
#include "skeleton.h"

inline unsigned int TextureFromFile(const char *path, const std::string &directory);
inline unsigned int TextureFromEmbedded(const aiTexture *texture);

class Model {
public:
    std::vector<Texture> textures_loaded;
    std::vector<Mesh> meshes;
    std::string directory;
    Skeleton skeleton; 
   
    glm::vec3 minBounds = glm::vec3(FLT_MAX);
    glm::vec3 maxBounds = glm::vec3(-FLT_MAX);

    Model(const std::string &path) {
        loadModel(path);
    }

    bool Loaded() const {
        return !meshes.empty();
    }

    bool HasBounds() const {
        return maxBounds.y > minBounds.y;
    }

    float Height() const {
        return HasBounds() ? maxBounds.y - minBounds.y : 1.0f;
    }

    float FloorY() const {
        return HasBounds() ? minBounds.y : 0.0f;
    }

    void Draw(Shader &shader) {
        for (unsigned int i = 0; i < meshes.size(); i++)
            meshes[i].Draw(shader);
    }

private:
    void loadModel(const std::string &path) {
        Assimp::Importer importer;
        const aiScene *scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_GenSmoothNormals);

        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
            std::cout << "ERROR::ASSIMP:: " << importer.GetErrorString() << std::endl;
            return;
        }

        directory = path.substr(0, path.find_last_of('/'));

        skeleton.Load(scene);
        processNode(scene->mRootNode, scene);
        computeBounds();
    }

    void processNode(aiNode *node, const aiScene *scene) {
        for (unsigned int i = 0; i < node->mNumMeshes; i++) {
            aiMesh *mesh = scene->mMeshes[node->mMeshes[i]];
            meshes.push_back(processMesh(mesh, scene));
        }

        for (unsigned int i = 0; i < node->mNumChildren; i++) {
            processNode(node->mChildren[i], scene);
        }
    }

    Mesh processMesh(aiMesh *mesh, const aiScene *scene) {
        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;
        std::vector<Texture> textures;

        for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
            Vertex vertex;
            glm::vec3 vector;

            vector.x = mesh->mVertices[i].x;
            vector.y = mesh->mVertices[i].y;
            vector.z = mesh->mVertices[i].z;
            vertex.Position = vector;

            if (mesh->HasNormals()) {
                vector.x = mesh->mNormals[i].x;
                vector.y = mesh->mNormals[i].y;
                vector.z = mesh->mNormals[i].z;
                vertex.Normal = vector;
            }

            if (mesh->mTextureCoords[0]) {
                glm::vec2 vec;
                vec.x = mesh->mTextureCoords[0][i].x;
                vec.y = mesh->mTextureCoords[0][i].y;
                vertex.TexCoords = vec;
            }
            else {
                vertex.TexCoords = glm::vec2(0.0f, 0.0f);
            }

            vertices.push_back(vertex);
        }

        assignBoneWeights(vertices, mesh);

        for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
            aiFace face = mesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; j++)
                indices.push_back(face.mIndices[j]);
        }

        aiMaterial *material = scene->mMaterials[mesh->mMaterialIndex];

        std::vector<Texture> diffuseMaps = loadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse", scene);
        if (diffuseMaps.empty())
            diffuseMaps = loadMaterialTextures(material, aiTextureType_BASE_COLOR, "texture_diffuse", scene);
        textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());

        std::vector<Texture> specularMaps = loadMaterialTextures(material, aiTextureType_SPECULAR, "texture_specular", scene);
        textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());

    
        aiColor4D colour(1.0f, 1.0f, 1.0f, 1.0f);
        if (material->Get(AI_MATKEY_BASE_COLOR, colour) != AI_SUCCESS)
            material->Get(AI_MATKEY_COLOR_DIFFUSE, colour);

        return Mesh(vertices, indices, textures, glm::vec4(colour.r, colour.g, colour.b, colour.a));
    }

    void computeBounds() {
        std::vector<glm::mat4> knochen;
        if (skeleton.HasBones())
            skeleton.ComputeBoneMatrices(Pose(), knochen);

        for (const Mesh &mesh : meshes) {
            for (const Vertex &v : mesh.vertices) {
                glm::vec3 p = v.Position;
                float gesamt = v.Weights.x + v.Weights.y + v.Weights.z + v.Weights.w;
                if (!knochen.empty() && gesamt > 0.0f) {
                    glm::mat4 skin = v.Weights.x * knochen[v.BoneIDs.x] + v.Weights.y * knochen[v.BoneIDs.y]
                                   + v.Weights.z * knochen[v.BoneIDs.z] + v.Weights.w * knochen[v.BoneIDs.w];
                    p = glm::vec3(skin * glm::vec4(p, 1.0f));
                }
                minBounds = glm::min(minBounds, p);
                maxBounds = glm::max(maxBounds, p);
            }
        }
    }

    void assignBoneWeights(std::vector<Vertex> &vertices, aiMesh *mesh) {
        for (unsigned int b = 0; b < mesh->mNumBones; b++) {
            const aiBone *bone = mesh->mBones[b];
            int id = skeleton.RegisterBone(bone);
            if (id >= Skeleton::MAX_BONES)
                continue;

            for (unsigned int w = 0; w < bone->mNumWeights; w++) {
                const aiVertexWeight &weight = bone->mWeights[w];
                if (weight.mWeight <= 0.0f)
                    continue;

                Vertex &vertex = vertices[weight.mVertexId];
                for (int slot = 0; slot < 4; slot++) {
                    if (vertex.Weights[slot] == 0.0f) {
                        vertex.BoneIDs[slot] = id;
                        vertex.Weights[slot] = weight.mWeight;
                        break;
                    }
                }
            }
        }

        for (Vertex &vertex : vertices) {
            float summe = vertex.Weights.x + vertex.Weights.y + vertex.Weights.z + vertex.Weights.w;
            if (summe > 0.0f)
                vertex.Weights /= summe;
        }
    }

    std::vector<Texture> loadMaterialTextures(aiMaterial *mat, aiTextureType type, std::string typeName, const aiScene *scene) {
        std::vector<Texture> textures;

        for (unsigned int i = 0; i < mat->GetTextureCount(type); i++) {
            aiString str;
            mat->GetTexture(type, i, &str);

            bool skip = false;
            for (unsigned int j = 0; j < textures_loaded.size(); j++) {
                if (std::strcmp(textures_loaded[j].path.data(), str.C_Str()) == 0) {
                    textures.push_back(textures_loaded[j]);
                    skip = true;
                    break;
                }
            }

            if (!skip) {
                Texture texture;

                const aiTexture *embedded = scene->GetEmbeddedTexture(str.C_Str());
                if (embedded)
                    texture.id = TextureFromEmbedded(embedded);
                else
                    texture.id = TextureFromFile(str.C_Str(), this->directory);

                texture.type = typeName;
                texture.path = str.C_Str();
                textures.push_back(texture);
                textures_loaded.push_back(texture);
            }
        }

        return textures;
    }
};

inline unsigned int CreateTexture(unsigned char *data, int width, int height, int nrComponents) {
    unsigned int textureID;
    glGenTextures(1, &textureID);

    GLenum format = GL_RGB;
    if (nrComponents == 1)
        format = GL_RED;
    else if (nrComponents == 3)
        format = GL_RGB;
    else if (nrComponents == 4)
        format = GL_RGBA;

    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    return textureID;
}

inline unsigned int TextureFromFile(const char *path, const std::string &directory) {
    std::string filename = directory + '/' + std::string(path);

    int width, height, nrComponents;
    unsigned char *data = stbi_load(filename.c_str(), &width, &height, &nrComponents, 0);

    if (!data) {
        std::cout << "Texture failed to load at path: " << filename << std::endl;
        return 0;
    }

    unsigned int textureID = CreateTexture(data, width, height, nrComponents);
    stbi_image_free(data);

    return textureID;
}

inline unsigned int TextureFromEmbedded(const aiTexture *texture) {
    int width, height, nrComponents;
    unsigned char *data = nullptr;

    if (texture->mHeight == 0) {

        data = stbi_load_from_memory(reinterpret_cast<const unsigned char *>(texture->pcData),
                                     texture->mWidth, &width, &height, &nrComponents, 0);
    }
    else {
        data = stbi_load_from_memory(reinterpret_cast<const unsigned char *>(texture->pcData),
                                     texture->mWidth * texture->mHeight, &width, &height, &nrComponents, 0);
    }

    if (!data) {
        std::cout << "Embedded texture failed to load" << std::endl;
        return 0;
    }

    unsigned int textureID = CreateTexture(data, width, height, nrComponents);
    stbi_image_free(data);

    return textureID;
}

#endif
