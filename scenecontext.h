#ifndef SCENECONTEXT_H
#define SCENECONTEXT_H

#include <string>

#include "background.h"
#include "debuggrid.h"
#include "resources.h"
#include "shader.h"
#include "walkdocument.h"

struct SceneContext {
    std::filesystem::path assetsDirectory;
    WalkDocumentStore documents;
    ResourceManager resources;
    Background background;
    Shader characterShader;
    DebugGrid grid;
    bool wegeAnzeigen = false;

    SceneContext(const std::string &assetPath)
        : assetsDirectory(std::filesystem::path(assetPath) / "assets"),
          documents(assetsDirectory), resources(assetPath),
          background(assetPath),
          characterShader((assetPath + "assets/shaders/model.vert").c_str(),
                          (assetPath + "assets/shaders/model.frag").c_str()),
          grid(assetPath) {
    }
};

#endif
