#ifndef NAVIGATIONEDITOR_H
#define NAVIGATIONEDITOR_H

#include <array>
#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "transitionentrance.h"
#include "walkeditorstate.h"

class Level01;

class NavigationEditor {
public:
    explicit NavigationEditor(Level01 &level);

    void Open();
    bool IsOpen() const { return open; }
    void RequestClose();
    void RequestQuit();
    bool ShouldQuit() const { return shouldQuit; }
    void Draw();
    void ImportDroppedFile(const std::string &path);

private:
    struct ModelFolder {
        std::map<std::string, ModelFolder> folders;
        std::vector<std::string> files;
    };
    struct BackgroundPreview {
        unsigned int texture = 0;
        glm::vec2 size{1920.0f, 1080.0f};
    };

    static ModelFolder BuildModelTree(const std::vector<std::string> &files);

    void Message(const std::string &text, bool error = false);
    bool HasChanges() const;
    bool IsSelected(WalkEditorState::PolygonType type, int index = -1) const;
    BackgroundPreview LoadBackground(const std::string &name);
    float ViewAspect() const;
    float HorizonPixel() const;
    void RefreshModelFiles();
    bool PropsLoadable();
    bool PropBounds(const std::string &model, glm::vec3 &minimum, glm::vec3 &maximum);
    void ChooseModel(const std::string &file);
    void DrawModelTree(const ModelFolder &folder);
    const TransitionEntrance &PreviewEntrance(int index);
    bool LoadDocument(const WalkDocument &document);
    void QueueAction(std::function<void()> action);
    void OpenDocument(const WalkDocument &initial, const std::string &sourceScene);
    bool Validate();
    bool Apply();
    bool Save();
    void SelectBackground(const std::string &name);
    void Import(const std::string &path);
    void ToolButton(const char *label, WalkEditorState::Tool tool);
    void FinishDraft();
    void DrawTransitions();
    void DrawProps();
    void DrawPanel();
    void DrawCanvas();
    void Shortcuts();
    void DrawConfirmation();
    void SetDirectory(const std::filesystem::path &directory);
    void DrawBrowser();

    Level01 &level;
    WalkEditorState state;
    BackgroundPreview background;
    std::string sceneLabel;
    bool open = false;
    bool initialized = false;
    bool fitRequested = true;
    bool dragging = false;
    bool showTransitionProperties = false;
    float zoom = 1.0f;
    glm::vec2 pan{0.0f};
    glm::vec2 dragOffset{0.0f};
    std::string status;
    bool statusError = false;

    std::function<void()> pendingAction;
    bool confirmRequested = false;
    bool pendingQuit = false;
    bool shouldQuit = false;
    bool browserRequested = false;
    std::filesystem::path browserDirectory;
    std::filesystem::path browserSelection;
    std::array<char, 4096> browserPath{};
   
    WalkDocument entranceDocument;
    float entranceAspect = 0.0f;
    int entranceIndex = -1;
    Navigation entranceNavigation;
    TransitionEntrance entrancePreview;
    std::vector<std::string> modelFiles;
    ModelFolder modelTree;
};

#endif
