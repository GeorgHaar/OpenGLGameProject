#ifndef LEVEL01_H
#define LEVEL01_H

#include <algorithm>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "assetfiles.h"
#include "scene.h"
#include "scenecontext.h"

class Level01 {
public:
    static constexpr const char *START_SCENE = "background:chapter1_crab_background00.png";

    Level01(const std::string &assetPath, float aspect) : context(assetPath), scene(context), viewAspect(aspect) {
        const std::vector<std::string> ids = SceneIds();
        if (std::find(ids.begin(), ids.end(), START_SCENE) != ids.end() && SwitchTo(START_SCENE)) return;
        for (const std::string &id : ids)
            if (id != START_SCENE && SwitchTo(id)) return;
        std::cout << "Kein spielbarer Raum gefunden. Bitte die gespeicherten Laufkarten in assets/navigation pruefen."
                  << std::endl;
    }

    bool Ready() const { return ready; }

    bool SwitchTo(const std::string &name) {
        if (name.rfind("background:", 0) != 0) {
            std::cout << "Unbekannte Zielszene: " << name << std::endl;
            return false;
        }
        std::string error;
        WalkDocument document;
        if (!LoadEditorDocument(name.substr(11), document, error)) {
            std::cout << "Szenenwechsel fehlgeschlagen: "
                      << (error.empty() ? "Fuer diesen Hintergrund fehlt eine gespeicherte Laufkarte." : error)
                      << std::endl;
            return false;
        }
        if (!scene.Enter(document, viewAspect, error)) {
            std::cout << "Szenenwechsel fehlgeschlagen: " << error << std::endl;
            return false;
        }
        ready = true;
        currentSceneId = name;
        std::cout << "Szene gewechselt: " << name << std::endl;
        return true;
    }

    bool LoadEditorDocument(const std::string &background, WalkDocument &document, std::string &error) {
        error.clear();
        const auto tested = sessionMaps.find(background);
        if (tested != sessionMaps.end()) {
            document = tested->second;
            return true;
        }
        return context.documents.Load(background, document, error);
    }

    void Update(float deltaTime) {
        scene.Update(deltaTime);
        const std::string target = scene.TakeTransition();
        if (!target.empty()) SwitchTo(target);
    }

    void Draw(int width, int height) {
        if (width <= 0 || height <= 0) return;
        const float aspect = static_cast<float>(width) / height;
        if (aspect != viewAspect) {
            viewAspect = aspect;
            std::string error;
            const WalkDocument document = scene.Document();
            if (!scene.Enter(document, viewAspect, error))
                std::cout << "Neues Fensterformat: " << error << std::endl;
        }
        scene.Draw();
    }

    void ClickAt(float mouseX, float mouseY, int width, int height, bool run) {
        scene.ClickAt(mouseX, mouseY, width, height, run);
    }
    void RaiseHand() { scene.RaiseHand(); }
    void StopWalking() { scene.StopWalking(); }
    bool HasTransitionAt(float x, float y, int width, int height) const {
        return scene.HasTransitionAt(x, y, width, height);
    }

    void ToggleNavigation() {
        context.wegeAnzeigen = !context.wegeAnzeigen;
        std::cout << (context.wegeAnzeigen
            ? "Laufbereich an: Gruen = Aussenrand, Rot = Hindernisse, Gelb = geplanter Weg."
            : "Laufbereich aus") << std::endl;
    }

    const WalkDocument &Document() const { return scene.Document(); }
    const std::string &CurrentSceneId() const { return currentSceneId; }

    bool ApplyDocument(const WalkDocument &document, std::string &error) {
        for (const SceneTransition &transition : document.transitions) {
            if (transition.targetScene.rfind("background:", 0) != 0) {
                error = "Unbekannte Zielszene beim Szenenwechsel '" + transition.name + "'. Bitte ein Ziel auswaehlen.";
                return false;
            }
            const std::string background = transition.targetScene.substr(11);
            if (background != document.background && !HasSavedMap(background)) {
                error = "Ziel von '" + transition.name +
                    "' ist nicht bereit: Bitte zuerst eine Laufkarte und einen Startpunkt fuer diesen Hintergrund speichern.";
                return false;
            }
        }
        if ("background:" + document.background == currentSceneId) {
            if (!scene.ApplyDocument(document, error)) return false;
        } else if (!ValidateWalkDocument(document, error)) {
            return false;
        }
        sessionMaps[document.background] = document;
        return true;
    }

    std::vector<std::string> SceneIds() const {
        std::vector<std::string> result;
        for (const std::string &background : ListBackgroundImages(context.assetsDirectory))
            if (HasSavedMap(background)) result.push_back("background:" + background);
        return result;
    }

    ResourceManager &Resources() { return context.resources; }
    WalkDocumentStore &Documents() { return context.documents; }
    const std::filesystem::path &AssetsDirectory() const { return context.assetsDirectory; }

private:
    bool HasSavedMap(const std::string &background) const {
        std::error_code error;
        return std::filesystem::is_regular_file(context.documents.PathFor(background), error);
    }

    SceneContext context;
    Scene scene;
    bool ready = false;
    std::string currentSceneId;
    float viewAspect;
    std::map<std::string, WalkDocument> sessionMaps;
};

#endif
