#ifndef RESOURCES_H
#define RESOURCES_H

#include <GL/glew.h>
#include <stb_image.h>

#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_map>

#include "model.h"
#include "assetfiles.h"

class ResourceManager {
public:
    struct TextureInfo {
        unsigned int id = 0;
        int width = 0;
        int height = 0;
    };

    ResourceManager(const std::string &assetPath) : assetPath(assetPath) {}

    Model &GetModel(const std::string &file) {
        auto found = models.find(file);
        if (found != models.end()) return found->second;
        Model &model = models.emplace(file, Model(ModelPath(file))).first->second;
        Report(file, model);
        return model;
    }

    bool ModelLoaded(const std::string &file) {
        return GetModel(file).Loaded();
    }

    void RetryFailedModels() {
        for (auto &entry : models) {
            if (entry.second.Loaded()) continue;
            const std::string path = ModelPath(entry.first);
            std::error_code error;
            if (!std::filesystem::is_regular_file(path, error)) continue;
            entry.second = Model(path);
            Report(entry.first, entry.second);
        }
    }

    TextureInfo GetTextureInfo(const std::string &file) {
        auto found = textures.find(file);
        if (found != textures.end()) return found->second;
        const TextureInfo info = loadTexture(assetPath + "assets/backgrounds/" + file);
        if (info.id != 0) textures[file] = info;
        return info;
    }

private:
    std::string assetPath;
    std::unordered_map<std::string, Model> models;
    std::unordered_map<std::string, TextureInfo> textures;

    std::string ModelPath(const std::string &file) const {
        const std::string path = assetPath + "assets/models/" + file;
        std::error_code error;
        if (std::filesystem::is_regular_file(path, error)) return path;
        const std::string located = LocateModelByFilename(assetPath + "assets", file);
        if (located.empty()) return path;
        std::cout << "Modell " << file << " fehlt unter assets/models, verwende stattdessen " << located << std::endl;
        return assetPath + "assets/models/" + located;
    }

    static void Report(const std::string &file, const Model &model) {
        if (model.Loaded())
            std::cout << "Modell geladen: " << file << " (" << model.Height() << " Modelleinheiten hoch)" << std::endl;
        else
            std::cout << "Modell konnte nicht geladen werden: " << file << std::endl;
    }

    TextureInfo loadTexture(const std::string &path) {
        int width, height, nrComponents;
        unsigned char *data = stbi_load(path.c_str(), &width, &height, &nrComponents, STBI_rgb_alpha);
        if (!data) {
            std::cout << "Textur konnte nicht geladen werden: " << path << std::endl;
            return {};
        }
        int maxSize = 0;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxSize);
        if (width > maxSize || height > maxSize) {
            stbi_image_free(data);
            std::cout << "Hintergrund ist zu gross fuer die Grafikkarte: " << path << std::endl;
            return {};
        }
        unsigned int textureID;
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        stbi_image_free(data);
        std::cout << "Textur geladen: " << path << " (" << width << "x" << height << ")" << std::endl;
        return {textureID, width, height};
    }
};

#endif
