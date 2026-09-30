#include "navigationeditor.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "assetfiles.h"
#include "level01.h"
#include "propplacement.h"

namespace fs = std::filesystem;
using Tool = WalkEditorState::Tool;
using PolygonType = WalkEditorState::PolygonType;

namespace {
constexpr ImU32 BOUNDARY_COLOR = IM_COL32(63, 222, 164, 255);
constexpr ImU32 OBSTACLE_COLOR = IM_COL32(255, 119, 114, 255);
constexpr ImU32 TRANSITION_COLOR = IM_COL32(205, 146, 255, 255);
constexpr ImU32 SELECTED_COLOR = IM_COL32(255, 214, 99, 255);
constexpr ImU32 DRAFT_COLOR = IM_COL32(94, 213, 250, 255);
constexpr ImU32 PROP_COLOR = IM_COL32(255, 166, 61, 255);
constexpr ImU32 PROP_GHOST_COLOR = IM_COL32(255, 166, 61, 140);
const ImVec4 WARNING_TEXT(1.0f, 0.60f, 0.54f, 1.0f);
const ImVec4 HINT_TEXT(1.0f, 0.85f, 0.46f, 1.0f);
const ImVec4 STATUS_TEXT(0.62f, 0.84f, 0.89f, 1.0f);
const ImVec4 STATUS_ERROR_TEXT(1.0f, 0.54f, 0.50f, 1.0f);

std::string SceneName(const std::string &id) {
    return id.rfind("background:", 0) == 0 ? "Hintergrund: " + id.substr(11) : id;
}

void SetBuffer(std::array<char, 4096> &buffer, const std::string &value) {
    const std::size_t length = std::min(value.size(), buffer.size() - 1);
    std::copy_n(value.data(), length, buffer.data());
    buffer[length] = '\0';
}

void ColoredText(const ImVec4 &color, const std::string &text) {
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::TextWrapped("%s", text.c_str());
    ImGui::PopStyleColor();
}

void PushEditorStyle() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 14));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.055f, 0.073f, 0.098f, 1));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.055f, 0.073f, 0.098f, 1));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.12f, 0.16f, 0.20f, 1));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.11f, 0.25f, 0.30f, 1));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.14f, 0.39f, 0.46f, 1));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.13f, 0.49f, 0.57f, 1));
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.11f, 0.32f, 0.37f, 1));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.15f, 0.41f, 0.47f, 1));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.13f, 0.49f, 0.57f, 1));
    ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.34f, 0.85f, 0.94f, 1));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.34f, 0.85f, 0.94f, 1));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.91f, 0.95f, 0.98f, 1));
}

void PopEditorStyle() {
    ImGui::PopStyleColor(12);
    ImGui::PopStyleVar(3);
}
} 

NavigationEditor::NavigationEditor(Level01 &level) : level(level) {
    const char *home = std::getenv("HOME");
    browserDirectory = home ? fs::path(home) : fs::current_path();
    SetBuffer(browserPath, browserDirectory.string());
}

NavigationEditor::ModelFolder NavigationEditor::BuildModelTree(const std::vector<std::string> &files) {
    ModelFolder root;
    for (const std::string &file : files) {
        ModelFolder *node = &root;
        std::size_t start = 0;
        for (std::size_t slash = file.find('/'); slash != std::string::npos; slash = file.find('/', start)) {
            node = &node->folders[file.substr(start, slash - start)];
            start = slash + 1;
        }
        node->files.push_back(file);
    }
    return root;
}

void NavigationEditor::Message(const std::string &text, bool error) {
    status = text;
    statusError = error;
}

bool NavigationEditor::HasChanges() const {
    return state.Dirty() || !state.draft.empty();
}

bool NavigationEditor::IsSelected(PolygonType type, int index) const {
    return state.selection.type == type && state.selection.index == index;
}

NavigationEditor::BackgroundPreview NavigationEditor::LoadBackground(const std::string &name) {
    const auto texture = level.Resources().GetTextureInfo(name);
    return {texture.id, glm::vec2(texture.width, texture.height)};
}


float NavigationEditor::ViewAspect() const {
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const glm::vec2 image = state.document.area.imageSize;
    return display.x > 0.0f && display.y > 0.0f ? display.x / display.y : image.x / image.y;
}


float NavigationEditor::HorizonPixel() const {
    return PropHorizonPixel(state.document.camera, state.document.area.imageSize);
}

void NavigationEditor::RefreshModelFiles() {
    level.Resources().RetryFailedModels();
    modelFiles = ListModelFiles(level.AssetsDirectory());
    modelTree = BuildModelTree(modelFiles);
}

bool NavigationEditor::PropsLoadable() {
    level.Resources().RetryFailedModels();
    for (const SceneProp &prop : state.document.props) {
        if (!level.Resources().ModelLoaded(prop.model)) {
            Message("Das 3D-Objekt \"" + prop.name + "\" hat kein ladbares Modell (assets/models/" + prop.model +
                    "). Wähle im Katalog ein anderes Modell und übernimm es, oder lösche das Objekt.", true);
            return false;
        }
    }
    return true;
}

bool NavigationEditor::PropBounds(const std::string &model, glm::vec3 &minimum, glm::vec3 &maximum) {
    Model &loaded = level.Resources().GetModel(model);
    minimum = loaded.minBounds;
    maximum = loaded.maxBounds;
    if (!ValidPropBounds(minimum, maximum)) DefaultPropBounds(minimum, maximum);
    return loaded.Loaded();
}

void NavigationEditor::ChooseModel(const std::string &file) {
    state.placementModel = file;
    if (!level.Resources().ModelLoaded(file)) {
        Message("Das Modell konnte nicht geladen werden: " + file, true);
        return;
    }
    if (state.GetTool() != Tool::Prop && !state.draft.empty())
        Message("Offene Kontur verworfen. Rückgängig macht abgeschlossene Änderungen rückgängig.");
    state.SetTool(Tool::Prop);
    Message("Auf den Boden klicken, um \"" + WalkEditorState::PropStem(file) + "\" zu platzieren.");
}

void NavigationEditor::DrawModelTree(const ModelFolder &folder) {
    for (const auto &[name, child] : folder.folders) {
        ImGui::PushID(name.c_str());
        if (child.folders.empty() && child.files.size() == 1) {
            const std::string &file = child.files.front();
            if (ImGui::Selectable(name.c_str(), state.placementModel == file)) ChooseModel(file);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", file.c_str());
        } else if (ImGui::TreeNodeEx(name.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth)) {
            DrawModelTree(child);
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    for (const std::string &file : folder.files) {
        const std::string label = WalkEditorState::PropStem(file);
        ImGui::PushID(file.c_str());
        if (ImGui::Selectable(label.c_str(), state.placementModel == file)) ChooseModel(file);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", file.c_str());
        ImGui::PopID();
    }
}

const TransitionEntrance &NavigationEditor::PreviewEntrance(int index) {
    const WalkDocument &document = state.document;
    const float aspect = ViewAspect();
    if (entranceDocument == document && entranceAspect == aspect && entranceIndex == index) return entrancePreview;
    const WalkArea &area = document.area;
    const Camera &camera = document.camera;
    if (entranceDocument.area != area || entranceDocument.camera != camera ||
        entranceDocument.spongebobSize != document.spongebobSize || entranceAspect != aspect) {
        std::vector<Polygon> obstacles;
        for (const Polygon &polygon : area.obstacles) obstacles.push_back(area.ToGround(polygon, camera, aspect));
        entranceNavigation.Build(area.ToGround(area.boundary, camera, aspect), obstacles,
                                 area.cellSize, area.clearance * std::max(1.0f, document.spongebobSize));
    }
    const glm::vec3 start = area.ToGround(document.spawn, camera, aspect);
    entrancePreview = ResolveTransitionEntrance(area, camera, aspect, entranceNavigation,
                                                {start.x, start.z}, document.transitions[index]);
    entranceDocument = document;
    entranceAspect = aspect;
    entranceIndex = index;
    return entrancePreview;
}

bool NavigationEditor::LoadDocument(const WalkDocument &document) {
    BackgroundPreview loaded = LoadBackground(document.background);
    if (!loaded.texture || loaded.size.x <= 0 || loaded.size.y <= 0) {
        Message("Das Hintergrundbild konnte nicht geöffnet werden.", true);
        return false;
    }
    const glm::vec2 previousSize = document.area.imageSize;
    if (!Finite(previousSize) || previousSize.x <= 0 || previousSize.y <= 0) {
        Message("Die Laufkarte hat keine gültige Bildgröße und konnte nicht geladen werden.", true);
        return false;
    }
    background = loaded;

    WalkDocument saved;
    std::string loadError;
    level.Documents().Load(document.background, saved, loadError);
    state.Load(saved);
    state.document = document;
    sceneLabel = "background:" + document.background;
    const bool resized = previousSize != background.size;
    if (resized) {
        const glm::vec2 factor = background.size / previousSize;
        for (glm::vec2 &point : state.document.area.boundary) point *= factor;
        for (Polygon &polygon : state.document.area.obstacles)
            for (glm::vec2 &point : polygon) point *= factor;
        for (SceneTransition &transition : state.document.transitions)
            for (glm::vec2 &point : transition.polygon) point *= factor;
        for (SceneProp &prop : state.document.props) prop.foot *= factor;
        state.document.spawn *= factor;
        state.document.area.imageSize = background.size;
    }
    initialized = true;
    open = true;
    fitRequested = true;
    dragging = false;
    RefreshModelFiles();
    Message(resized ? "Die Bildauflösung wurde geändert. Konturen und Startpunkt wurden proportional angepasst; bitte speichern."
                    : "Konturen auswählen oder mit einem Werkzeug neu einzeichnen.");
    return true;
}

void NavigationEditor::QueueAction(std::function<void()> action) {
    if (initialized && HasChanges()) {
        pendingAction = std::move(action);
        pendingQuit = false;
        confirmRequested = true;
    } else {
        action();
    }
}

void NavigationEditor::OpenDocument(const WalkDocument &initial, const std::string &sourceScene) {
    open = true;
    const bool sameSource = sceneLabel == sourceScene;
    if (initialized && sameSource && state.document.background == initial.background && HasChanges())
        return;
    WalkDocument candidate = initial;
    WalkDocument stored;
    std::string loadError;
    if (level.Documents().Load(initial.background, stored, loadError)) {
        const BackgroundPreview currentImage = LoadBackground(initial.background);
        if (currentImage.texture && stored.area.imageSize != currentImage.size) candidate = stored;
    }
    if (!initialized || (sameSource && state.document.background == initial.background))
        LoadDocument(candidate);
    else
        QueueAction([this, candidate] { LoadDocument(candidate); });
}

bool NavigationEditor::Validate() {
    if (dragging) {
        std::string error;
        dragging = false;
        if (!state.EndDrag(error)) {
            Message(error, true);
            return false;
        }
    }
    state.EndEdit();
    if (!state.draft.empty()) {
        Message("Die Kontur ist noch offen. Mit Enter abschließen oder mit Esc verwerfen.", true);
        return false;
    }
    std::string error;
    if (!ValidateWalkDocument(state.document, error)) {
        Message(error, true);
        return false;
    }
    return PropsLoadable();
}

bool NavigationEditor::Apply() {
    if (!initialized || !Validate()) return false;
    std::string error;
    if (!level.ApplyDocument(state.document, error)) {
        Message(error.empty() ? "Die Karte konnte nicht übernommen werden." : error, true);
        return false;
    }
    return true;
}

bool NavigationEditor::Save() {
    if (!Apply()) return false;
    std::string error;
    if (!level.Documents().Save(state.document, error)) {
        Message("Änderungen übernommen, Speichern fehlgeschlagen: " + error, true);
        return false;
    }
    state.MarkSaved();
    Message(level.CurrentSceneId() == sceneLabel ? "Gespeichert und im Spiel übernommen."
                                                 : "Raum gespeichert. Mit \"Im Spiel testen\" betreten.");
    return true;
}

void NavigationEditor::RequestClose() {
    if (pendingAction || !open || !Apply()) return;
    if (level.CurrentSceneId() != sceneLabel && !level.SwitchTo(sceneLabel)) {
        Message("Der Raum konnte nicht geöffnet werden. Prüfe seine Laufkarte und den Startpunkt.", true);
        return;
    }
    open = false;
    Message(state.Dirty() ? "Im Spiel übernommen. Die Änderungen sind noch nicht gespeichert."
                          : "Die Karte ist gespeichert.");
}

void NavigationEditor::RequestQuit() {
    if (!initialized || !HasChanges()) {
        shouldQuit = true;
        return;
    }
    open = true;
    pendingQuit = true;
    pendingAction = [this] { shouldQuit = true; open = false; };
    confirmRequested = true;
}

void NavigationEditor::SelectBackground(const std::string &name) {
    WalkDocument document;
    std::string error;
    if (level.LoadEditorDocument(name, document, error)) {
        LoadDocument(document);
        return;
    }
    if (!error.empty()) {
        Message("Diese Laufkarte konnte nicht geladen werden: " + error, true);
        return;
    }
    const BackgroundPreview loaded = LoadBackground(name);
    if (!loaded.texture || loaded.size.x <= 0 || loaded.size.y <= 0) {
        Message("Das Hintergrundbild konnte nicht geöffnet werden.", true);
        return;
    }
    document.background = name;
    document.area.imageSize = loaded.size;
    const float w = loaded.size.x, h = loaded.size.y;
    document.area.boundary = {{0.03f * w, 0.65f * h}, {0.97f * w, 0.65f * h},
                              {0.97f * w, 0.97f * h}, {0.03f * w, 0.97f * h}};
    document.spawn = {0.5f * w, 0.85f * h};
    LoadDocument(document);
    Message("Neue Laufkarte: den grünen Außenrand an die begehbare Fläche anpassen.");
}

void NavigationEditor::Import(const std::string &path) {
    QueueAction([this, path] {
        std::string error;
        const std::string name = ImportBackgroundImage(level.AssetsDirectory(), path, error);
        if (name.empty()) {
            Message(error.empty() ? "Das Bild konnte nicht importiert werden." : error, true);
            return;
        }
        SelectBackground(name);
    });
}

void NavigationEditor::ToolButton(const char *label, Tool tool) {
    const bool active = state.GetTool() == tool;
    if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.48f, 0.56f, 1));
    if (ImGui::Button(label, ImVec2(-1, 30))) {
        if (state.GetTool() != tool && !state.draft.empty())
            Message("Offene Kontur verworfen. Rückgängig macht abgeschlossene Änderungen rückgängig.");
        state.SetTool(tool);
        if (tool == Tool::Transition)
            Message("Mindestens drei Punkte zeichnen. Danach einen Namen und die Zielszene wählen.");
    }
    if (active) ImGui::PopStyleColor();
}

void NavigationEditor::FinishDraft() {
    const bool transition = state.GetTool() == Tool::Transition;
    std::string error;
    if (!state.FinishPolygon(error)) {
        Message(error, true);
    } else if (transition) {
        state.SetTool(Tool::Select);
        showTransitionProperties = true;
        Message("Szenenwechsel eingezeichnet. Jetzt links die Zielszene wählen.");
    } else {
        Message("Kontur abgeschlossen.");
    }
}

void NavigationEditor::DrawTransitions() {
    ImGui::SeparatorText("Szenenwechsel");
    const float listHeight = std::clamp(
        float(state.document.transitions.size()) * ImGui::GetTextLineHeightWithSpacing() + 8.0f, 46.0f, 100.0f);
    if (ImGui::BeginListBox("##Szenenwechsel", ImVec2(-1, listHeight))) {
        if (state.document.transitions.empty()) ImGui::TextDisabled("Noch kein Szenenwechsel");
        for (std::size_t i = 0; i < state.document.transitions.size(); ++i) {
            const SceneTransition &transition = state.document.transitions[i];
            const std::string label = transition.name.empty() ? "Übergang " + std::to_string(i + 1) : transition.name;
            ImGui::PushID(int(i));
            if (ImGui::Selectable(label.c_str(), IsSelected(PolygonType::Transition, int(i)))) {
                state.SetTool(Tool::Select);
                state.SelectTransition(i);
            }
            ImGui::PopID();
        }
        ImGui::EndListBox();
    }
    SceneTransition *transition = state.SelectedTransition();
    if (!transition) return;
    if (showTransitionProperties) {
        ImGui::SetScrollHereY(0.45f);
        showTransitionProperties = false;
    }
    ImGui::PushID(state.selection.index);
    ImGui::TextUnformatted("Name");
    std::array<char, 4096> name{};
    SetBuffer(name, transition->name);
    ImGui::SetNextItemWidth(-1);
    const bool renamed = ImGui::InputText("##Name", name.data(), name.size());
    if (ImGui::IsItemActivated()) state.BeginEdit();
    if (renamed) transition->name = name.data();
    if (ImGui::IsItemDeactivated()) state.EndEdit();

    const std::vector<std::string> scenes = level.SceneIds();
    std::string selectedLabel = transition->targetScene.empty() ? "Zielszene wählen ..." : SceneName(transition->targetScene);
    if (transition->targetScene == sceneLabel) selectedLabel += " (aktuelle Szene)";
    ImGui::TextUnformatted("Zielszene");
    ImGui::SetNextItemWidth(-1);
    ImGui::BeginDisabled(scenes.empty());
    if (ImGui::BeginCombo("##Zielszene", selectedLabel.c_str())) {
        for (const std::string &scene : scenes) {
            const bool selected = scene == transition->targetScene;
            std::string label = SceneName(scene);
            if (scene == sceneLabel) label += " (aktuelle Szene)";
            ImGui::PushID(scene.c_str());
            if (ImGui::Selectable(label.c_str(), selected) && !selected) {
                state.BeginEdit();
                transition->targetScene = scene;
                state.EndEdit();
                Message("Zielszene gewählt. SpongeBob nutzt den markierten Eingang für den Szenenwechsel.");
            }
            if (selected) ImGui::SetItemDefaultFocus();
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    ImGui::EndDisabled();
    if (scenes.empty()) ImGui::TextWrapped("Es ist noch keine Zielszene verfügbar. Lege zuerst eine weitere Szene an.");
    else if (transition->targetScene.empty()) ImGui::TextWrapped("Zum Speichern eine Zielszene auswählen.");
    const bool returnsToStart = transition->targetScene == sceneLabel;
    if (returnsToStart) ImGui::TextWrapped("Diese Tür setzt SpongeBob an den Startpunkt dieser Szene zurück.");
    const TransitionEntrance &entrance = PreviewEntrance(state.selection.index);
    if (!entrance.reachable) {
        ColoredText(WARNING_TEXT, "Kein erreichbarer Eingang. Passe den Außenrand so an, dass begehbarer Boden näher an die Tür reicht.");
    } else {
        ColoredText(HINT_TEXT, entrance.automatic ? "Eingang automatisch vor der Tür" : "Eingang in der Türfläche erreichbar");
        if (!returnsToStart) ImGui::TextWrapped("SpongeBob läuft zum markierten Eingang und wechselt dort die Szene.");
    }
    ImGui::PopID();
}

void NavigationEditor::DrawProps() {
    ImGui::SeparatorText("3D-Objekte");
    ImGui::TextUnformatted("Modelle aus assets/models");
    ImGui::SameLine();
    if (ImGui::SmallButton("Aktualisieren")) {
        RefreshModelFiles();
        Message(std::to_string(modelFiles.size()) + " Modelle gefunden.");
    }
    if (ImGui::BeginChild("Modellkatalog", ImVec2(0, 150), ImGuiChildFlags_Borders)) {
        if (modelFiles.empty()) ImGui::TextWrapped("Keine .glb- oder .gltf-Dateien unter assets/models gefunden.");
        else DrawModelTree(modelTree);
    }
    ImGui::EndChild();
    if (state.placementModel.empty()) {
        ImGui::TextDisabled("Noch kein Modell gewählt.");
    } else {
        glm::vec3 minimum, maximum;
        if (PropBounds(state.placementModel, minimum, maximum))
            ImGui::TextWrapped("Gewählt: %s", WalkEditorState::PropStem(state.placementModel).c_str());
        else
            ColoredText(WARNING_TEXT, state.placementModel + " konnte nicht geladen werden.");
    }
    ToolButton("Objekt platzieren", Tool::Prop);
    ImGui::SetNextItemWidth(-1);
    ImGui::DragFloat("##Platzierhoehe", &state.placementHeight, 0.01f, PROP_MIN_HEIGHT, 10.0f,
                     "Höhe neuer Objekte: %.2f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::TextDisabled("1,00 = SpongeBob-Höhe bei 100 %%");

    ImGui::Spacing();
    ImGui::TextUnformatted("Platzierte Objekte");
    const float listHeight = std::clamp(
        float(state.document.props.size()) * ImGui::GetTextLineHeightWithSpacing() + 8.0f, 46.0f, 120.0f);
    if (ImGui::BeginListBox("##Objekte", ImVec2(-1, listHeight))) {
        if (state.document.props.empty()) ImGui::TextDisabled("Noch kein Objekt platziert");
        for (std::size_t i = 0; i < state.document.props.size(); ++i) {
            const SceneProp &prop = state.document.props[i];
            const std::string label = prop.name.empty() ? "Objekt " + std::to_string(i + 1) : prop.name;
            const bool loadable = level.Resources().ModelLoaded(prop.model);
            ImGui::PushID(int(i));
            if (!loadable) ImGui::PushStyleColor(ImGuiCol_Text, WARNING_TEXT);
            if (ImGui::Selectable(label.c_str(), IsSelected(PolygonType::Prop, int(i)))) {
                state.SetTool(Tool::Select);
                state.SelectProp(i);
            }
            if (!loadable) {
                ImGui::PopStyleColor();
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Modell fehlt: %s", prop.model.c_str());
            }
            ImGui::PopID();
        }
        ImGui::EndListBox();
    }
    SceneProp *prop = state.SelectedProp();
    if (!prop) return;
    ImGui::PushID(state.selection.index);
    ImGui::TextUnformatted("Name");
    std::array<char, 4096> name{};
    SetBuffer(name, prop->name);
    ImGui::SetNextItemWidth(-1);
    const bool renamed = ImGui::InputText("##Objektname", name.data(), name.size());
    if (ImGui::IsItemActivated()) state.BeginEdit();
    if (renamed) prop->name = name.data();
    if (ImGui::IsItemDeactivated()) state.EndEdit();

    ImGui::TextUnformatted("Modell");
    ImGui::TextWrapped("%s", prop->model.c_str());
    glm::vec3 minimum, maximum;
    if (!PropBounds(prop->model, minimum, maximum))
        ColoredText(WARNING_TEXT, "Dieses Modell konnte nicht geladen werden. Wähle ein anderes aus dem Katalog.");
    ImGui::BeginDisabled(state.placementModel.empty() || state.placementModel == prop->model);
    if (ImGui::Button("Gewähltes Modell übernehmen", ImVec2(-1, 0))) {
        state.BeginEdit();
        prop->model = state.placementModel;
        state.EndEdit();
        Message("Modell ersetzt.");
    }
    ImGui::EndDisabled();

    ImGui::TextUnformatted("Fußpunkt X / Y im Bild");
    float foot[2] = {prop->foot.x, prop->foot.y};
    ImGui::SetNextItemWidth(-1);
    const bool moved = ImGui::DragFloat2("##Fusspunkt", foot, 1.0f, -1e7f, 1e7f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
    if (ImGui::IsItemActivated()) state.BeginEdit();
    if (moved) state.MoveSelected({foot[0], foot[1]});
    if (ImGui::IsItemDeactivated()) state.EndEdit();
    if (prop->foot.y <= HorizonPixel())
        ColoredText(WARNING_TEXT, "Der Fußpunkt liegt am oder über dem Horizont und wäre im Spiel unsichtbar. Zum Speichern unter die orangefarbene Linie schieben.");

    ImGui::TextUnformatted("Höhe");
    float height = prop->height;
    ImGui::SetNextItemWidth(-1);
    const bool resized = ImGui::DragFloat("##Objekthoehe", &height, 0.01f, PROP_MIN_HEIGHT, PROP_MAX_HEIGHT,
                                          "%.2f", ImGuiSliderFlags_AlwaysClamp);
    if (ImGui::IsItemActivated()) state.BeginEdit();
    if (resized) prop->height = height;
    if (ImGui::IsItemDeactivated()) state.EndEdit();
    ImGui::TextDisabled("1,00 = SpongeBob-Höhe bei 100 %%");

    ImGui::TextUnformatted("Drehung");
    float rotation = prop->rotation;
    ImGui::SetNextItemWidth(-1);
    const bool rotated = ImGui::SliderFloat("##Objektdrehung", &rotation, -180.0f, 180.0f, "%.0f°", ImGuiSliderFlags_AlwaysClamp);
    if (ImGui::IsItemActivated()) state.BeginEdit();
    if (rotated) prop->rotation = rotation;
    if (ImGui::IsItemDeactivated()) state.EndEdit();

    if (ImGui::Button("Objekt löschen   Entf", ImVec2(-1, 0))) {
        state.DeleteSelectedProp();
        Message("Objekt gelöscht. Mit Strg+Z rückgängig machen.");
    }
    ImGui::PopID();
}

void NavigationEditor::DrawPanel() {
    ImGui::TextUnformatted("HINTERGRUND");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo("##Hintergrund", state.document.background.c_str())) {
        for (const std::string &name : ListBackgroundImages(level.AssetsDirectory())) {
            const bool selected = name == state.document.background;
            if (ImGui::Selectable(name.c_str(), selected) && !selected)
                QueueAction([this, name] { SelectBackground(name); });
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    if (ImGui::Button("Bild importieren ...", ImVec2(-1, 0))) browserRequested = true;
    ImGui::TextDisabled("PNG / JPG / BMP / TGA\nAuch per Drag & Drop");
    ImGui::Spacing();
    ImGui::SeparatorText("SpongeBob");
    ImGui::TextUnformatted("Größe");
    float sizePercent = state.document.spongebobSize * 100.0f;
    ImGui::SetNextItemWidth(-1);
    const bool sizeChanged = ImGui::SliderFloat("##SpongeBobSize", &sizePercent, 25.0f, 400.0f, "%.0f %%", ImGuiSliderFlags_AlwaysClamp);
    if (ImGui::IsItemActivated()) state.BeginEdit();
    if (sizeChanged) state.document.spongebobSize = sizePercent / 100.0f;
    if (ImGui::IsItemDeactivated()) state.EndEdit();
    ImGui::TextWrapped("Größe in dieser Szene. Mit \"Im Spiel testen\" ansehen.");
    ImGui::Spacing();
    ImGui::SeparatorText("Zeichnen");
    ToolButton("Auswählen", Tool::Select);
    ToolButton("Außenrand", Tool::Boundary);
    ToolButton("Hindernis", Tool::Obstacle);
    ToolButton("Szenenwechsel", Tool::Transition);
    ToolButton("Startpunkt", Tool::Spawn);
    if (!state.draft.empty()) {
        ImGui::Text("%d Punkte in offener Kontur", int(state.draft.size()));
        if (ImGui::Button("Abschließen (Enter)", ImVec2(-1, 0))) FinishDraft();
        if (ImGui::Button("Offene Kontur verwerfen", ImVec2(-1, 0))) state.CancelDraft();
    }

    ImGui::SeparatorText("Konturen");
    if (ImGui::BeginListBox("##Konturen", ImVec2(-1, 120))) {
        if (ImGui::Selectable("Außenrand", IsSelected(PolygonType::Boundary))) {
            state.SetTool(Tool::Select);
            state.SelectBoundary();
        }
        for (std::size_t i = 0; i < state.document.area.obstacles.size(); ++i) {
            const std::string label = "Hindernis " + std::to_string(i + 1);
            if (ImGui::Selectable(label.c_str(), IsSelected(PolygonType::Obstacle, int(i)))) {
                state.SetTool(Tool::Select);
                state.SelectObstacle(i);
            }
        }
        ImGui::EndListBox();
    }
    DrawTransitions();
    ImGui::BeginDisabled(state.SelectedPolygon() == nullptr);
    if (ImGui::Button("Kontur löschen", ImVec2(-1, 0))) {
        state.DeleteSelectedPolygon();
        Message("Kontur gelöscht. Mit Strg+Z rückgängig machen.");
    }
    ImGui::EndDisabled();

    const Polygon *polygon = state.SelectedPolygon();
    if (polygon && state.selection.vertex >= 0 && state.selection.vertex < int(polygon->size())) {
        const glm::vec2 point = (*polygon)[state.selection.vertex];
        float coordinates[2] = {point.x, point.y};
        ImGui::SetNextItemWidth(-1);
        const bool changed = ImGui::DragFloat2("##Punkt", coordinates, 1.0f, 0.0f, 0.0f, "%.1f");
        if (ImGui::IsItemActivated()) state.BeginEdit();
        if (changed) state.MoveSelected({coordinates[0], coordinates[1]});
        if (ImGui::IsItemDeactivated()) {
            std::string error;
            if (!state.EndDrag(error)) Message(error, true);
        }
        ImGui::TextDisabled("Punktposition X / Y im Bild");
    }

    ImGui::Spacing();
    DrawProps();

    ImGui::Spacing();
    ImGui::SeparatorText("Änderungen");
    const float half = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    ImGui::BeginDisabled(!state.CanUndo());
    if (ImGui::Button("Rückgängig", ImVec2(half, 0))) state.Undo();
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!state.CanRedo());
    if (ImGui::Button("Wiederholen", ImVec2(half, 0))) state.Redo();
    ImGui::EndDisabled();
    ImGui::TextDisabled("%s", HasChanges() ? "Noch nicht gespeichert" : "Keine ungespeicherten Änderungen");
    ImGui::Spacing();
    ImGui::SeparatorText("Bedienung");
    ImGui::TextWrapped("Punkte anklicken, mit Enter oder dem ersten Punkt schließen.");
    ImGui::TextWrapped("Szenenwechsel: eine begehbare Fläche umrahmen und anschließend ihre Zielszene wählen.");
    ImGui::TextWrapped("Auswählen: Punkte ziehen. Umschalt + Klick auf eine Kante fügt einen Punkt ein. Entf löscht den ausgewählten Punkt.");
    ImGui::TextWrapped("3D-Objekte: Modell im Katalog wählen, dann auf den Boden klicken. Im Werkzeug \"Auswählen\" lässt sich der Fußpunkt ziehen.");
    ImGui::TextWrapped("Mausrad: Zoom · mittlere Maustaste: verschieben · Strg+Z / Strg+Y: rückgängig / wiederholen.");
}

void NavigationEditor::DrawCanvas() {
    if (ImGui::Button("Bild einpassen")) fitRequested = true;
    ImGui::SameLine();
    ImGui::TextDisabled("%.0f × %.0f  ·  %.0f %%", background.size.x, background.size.y, zoom * 100.0f);
    ImGui::SameLine();
    const Tool tool = state.GetTool();
    ImGui::TextColored(ImVec4(0.4f, 0.85f, 0.94f, 1), "%s", tool == Tool::Select ? "Auswählen"
        : tool == Tool::Boundary ? "Außenrand zeichnen"
        : tool == Tool::Obstacle ? "Hindernis zeichnen"
        : tool == Tool::Transition ? "Szenenwechsel zeichnen"
        : tool == Tool::Prop ? "Objekt platzieren" : "Startpunkt setzen");

    ImVec2 available = ImGui::GetContentRegionAvail();
    available.x = std::max(available.x, 1.0f);
    available.y = std::max(available.y, 1.0f);
    ImGui::InvisibleButton("##Zeichenflaeche", available, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);
    const ImVec2 origin = ImGui::GetItemRectMin();
    const ImVec2 end = ImGui::GetItemRectMax();
    const bool hovered = ImGui::IsItemHovered();
    const ImGuiIO &io = ImGui::GetIO();
    const glm::vec2 mouse(io.MousePos.x - origin.x, io.MousePos.y - origin.y);
    if (fitRequested) {
        zoom = std::max(0.01f, std::min((available.x - 32.0f) / background.size.x, (available.y - 32.0f) / background.size.y));
        pan = (glm::vec2(available.x, available.y) - background.size * zoom) * 0.5f;
        fitRequested = false;
    }
    if (hovered && io.MouseWheel != 0.0f) {
        const glm::vec2 anchor = (mouse - pan) / zoom;
        zoom = glm::clamp(zoom * std::pow(1.15f, io.MouseWheel), 0.02f, 12.0f);
        pan = mouse - anchor * zoom;
    }
    if (hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f))
        pan += glm::vec2(io.MouseDelta.x, io.MouseDelta.y);

    const glm::vec2 pixel = (mouse - pan) / zoom;
    const bool inside = pixel.x >= 0 && pixel.y >= 0 && pixel.x <= background.size.x && pixel.y <= background.size.y;
    if (hovered && (inside || tool == Tool::Select) && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        std::string error;
        if (tool == Tool::Select) {
            if (io.KeyShift && state.HitEdge(pixel, 9.0f / zoom)) {
                if (!state.InsertVertex(pixel, error)) Message(error, true);
            } else if (state.HitVertex(pixel, 9.0f / zoom)) {
                state.BeginEdit();
                dragOffset = (*state.SelectedPolygon())[state.selection.vertex] - pixel;
                dragging = true;
            } else if (state.HitProp(pixel, 14.0f / zoom)) {
                state.BeginEdit();
                dragOffset = state.SelectedProp()->foot - pixel;
                dragging = true;
            } else if (!state.HitPolygon(pixel)) {
                state.ClearSelection();
            }
        } else if ((tool == Tool::Boundary || tool == Tool::Obstacle || tool == Tool::Transition) &&
                   state.draft.size() >= 3 && glm::distance(pixel, state.draft.front()) * zoom < 10.0f) {
            FinishDraft();
        } else if (!state.Click(pixel, error)) {
            Message(error, true);
        } else if (tool == Tool::Spawn) {
            Message("Startpunkt gesetzt.");
        } else if (tool == Tool::Prop) {
            Message(pixel.y <= HorizonPixel()
                ? "Objekt platziert, aber am oder über dem Horizont; dort wäre es im Spiel unsichtbar. Zum Speichern unter die orangefarbene Linie schieben."
                : "Objekt platziert. Name, Höhe und Drehung links anpassen; weitere Klicks setzen weitere Objekte.");
        }
    }
    if (dragging) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            state.MoveSelected(pixel + dragOffset);
        } else {
            std::string error;
            if (!state.EndDrag(error)) Message(error, true);
            dragging = false;
        }
    }

    ImDrawList *draw = ImGui::GetWindowDrawList();
    draw->PushClipRect(origin, end, true);
    draw->AddRectFilled(origin, end, IM_COL32(12, 17, 24, 255));
    const auto screen = [&](glm::vec2 p) {
        return ImVec2(origin.x + pan.x + p.x * zoom, origin.y + pan.y + p.y * zoom);
    };
    draw->AddImage(static_cast<ImTextureID>(background.texture), screen({0, 0}), screen(background.size), ImVec2(0, 1), ImVec2(1, 0));
    draw->AddRect(screen({0, 0}), screen(background.size), IM_COL32(83, 102, 125, 255));

    const auto outline = [&](const Polygon &points, PolygonType type, int index, ImU32 color) {
        const bool selected = IsSelected(type, index);
        for (std::size_t i = 0; i < points.size(); ++i) {
            draw->AddLine(screen(points[i]), screen(points[(i + 1) % points.size()]),
                          selected ? SELECTED_COLOR : color, selected ? 3.0f : 2.0f);
            if (tool == Tool::Select) {
                const bool selectedVertex = selected && state.selection.vertex == int(i);
                const float radius = selectedVertex ? 6.0f : selected ? 4.5f : 3.0f;
                draw->AddCircleFilled(screen(points[i]), radius + 1.5f, IM_COL32(10, 18, 24, 255));
                draw->AddCircleFilled(screen(points[i]), radius, selected ? SELECTED_COLOR : color);
            }
        }
    };
    outline(state.document.area.boundary, PolygonType::Boundary, -1, BOUNDARY_COLOR);
    for (std::size_t i = 0; i < state.document.area.obstacles.size(); ++i)
        outline(state.document.area.obstacles[i], PolygonType::Obstacle, int(i), OBSTACLE_COLOR);
    for (std::size_t i = 0; i < state.document.transitions.size(); ++i) {
        const SceneTransition &transition = state.document.transitions[i];
        outline(transition.polygon, PolygonType::Transition, int(i), TRANSITION_COLOR);
        if (transition.polygon.empty()) continue;
        glm::vec2 center(0.0f);
        for (glm::vec2 point : transition.polygon) center += point;
        center /= float(transition.polygon.size());
        const std::string label = transition.name.empty() ? "Übergang " + std::to_string(i + 1) : transition.name;
        const ImVec2 labelSize = ImGui::CalcTextSize(label.c_str());
        const ImVec2 anchor = screen(center);
        const ImVec2 text(anchor.x - labelSize.x * 0.5f, anchor.y - labelSize.y * 0.5f);
        draw->AddRectFilled(ImVec2(text.x - 5, text.y - 3), ImVec2(text.x + labelSize.x + 5, text.y + labelSize.y + 3),
                            IM_COL32(32, 19, 46, 225), 3);
        draw->AddText(text, TRANSITION_COLOR, label.c_str());
    }

    const float aspect = ViewAspect();
    const WalkArea &area = state.document.area;
    const Camera &camera = state.document.camera;
    if (tool == Tool::Prop || state.SelectedProp()) {
        const float row = HorizonPixel();
        draw->AddLine(screen({0.0f, row}), screen({background.size.x, row}), PROP_GHOST_COLOR, 1.0f);
        draw->AddText(ImVec2(screen({0.0f, row}).x + 6, screen({0.0f, row}).y - 18), PROP_GHOST_COLOR, "Objekte nur unterhalb dieser Linie");
    }
    const auto propBox = [&](const SceneProp &prop, ImU32 color, float thickness) {
        glm::vec3 minimum, maximum;
        PropBounds(prop.model, minimum, maximum);
        const glm::vec3 ground = area.ToGround(prop.foot, camera, aspect);
        const auto corners = PropBoxCorners(minimum, maximum, PropTransform(minimum, maximum, ground, prop.height, prop.rotation));
        std::array<ImVec2, 8> points;
        for (std::size_t i = 0; i < corners.size(); ++i) points[i] = screen(area.ToImage(corners[i], camera, aspect));
        for (std::size_t i = 0; i < 4; ++i) {
            const std::size_t next = (i + 1) % 4;
            draw->AddLine(points[i], points[next], color, thickness + 1.0f);
            draw->AddLine(points[i + 4], points[next + 4], color, thickness);
            draw->AddLine(points[i], points[i + 4], color, thickness);
        }
    };
    const auto propMarker = [&](glm::vec2 foot, ImU32 color, float radius, const std::string &label) {
        const ImVec2 center = screen(foot);
        const ImVec2 diamond[4] = {ImVec2(center.x, center.y - radius), ImVec2(center.x + radius, center.y),
                                   ImVec2(center.x, center.y + radius), ImVec2(center.x - radius, center.y)};
        draw->AddConvexPolyFilled(diamond, 4, IM_COL32(24, 18, 10, 235));
        draw->AddPolyline(diamond, 4, color, ImDrawFlags_Closed, 2.0f);
        if (label.empty()) return;
        const ImVec2 size = ImGui::CalcTextSize(label.c_str());
        const ImVec2 text(center.x + radius + 5, center.y - size.y * 0.5f);
        draw->AddRectFilled(ImVec2(text.x - 4, text.y - 2), ImVec2(text.x + size.x + 4, text.y + size.y + 2), IM_COL32(40, 26, 8, 225), 3);
        draw->AddText(text, color, label.c_str());
    };
    for (std::size_t i = 0; i < state.document.props.size(); ++i) {
        const SceneProp &prop = state.document.props[i];
        const bool selected = IsSelected(PolygonType::Prop, int(i));
        const ImU32 color = selected ? SELECTED_COLOR : PROP_COLOR;
        propBox(prop, color, selected ? 2.0f : 1.0f);
        propMarker(prop.foot, color, selected ? 8.0f : 6.0f, prop.name.empty() ? "Objekt " + std::to_string(i + 1) : prop.name);
    }
    if (tool == Tool::Prop && hovered && inside && !state.placementModel.empty()) {
        SceneProp ghost;
        ghost.model = state.placementModel;
        ghost.foot = pixel;
        ghost.height = state.placementHeight;
        propBox(ghost, PROP_GHOST_COLOR, 1.0f);
        propMarker(pixel, PROP_GHOST_COLOR, 6.0f, {});
    }

    const ImU32 draftColor = tool == Tool::Transition ? TRANSITION_COLOR : DRAFT_COLOR;
    for (std::size_t i = 0; i < state.draft.size(); ++i) {
        if (i > 0) draw->AddLine(screen(state.draft[i - 1]), screen(state.draft[i]), draftColor, 2.5f);
        draw->AddCircleFilled(screen(state.draft[i]), 4.5f, draftColor);
    }
    if (!state.draft.empty() && hovered && inside) {
        draw->AddLine(screen(state.draft.back()), screen(pixel), draftColor, 1.5f);
        if (state.draft.size() >= 3) draw->AddCircle(screen(state.draft.front()), 9, draftColor, 0, 2);
    }
    const ImVec2 spawn = screen(state.document.spawn);
    draw->AddCircleFilled(spawn, 8, IM_COL32(14, 25, 30, 255));
    draw->AddCircle(spawn, 7, DRAFT_COLOR, 0, 2.5f);
    draw->AddLine(ImVec2(spawn.x - 11, spawn.y), ImVec2(spawn.x + 11, spawn.y), DRAFT_COLOR, 2);
    draw->AddLine(ImVec2(spawn.x, spawn.y - 11), ImVec2(spawn.x, spawn.y + 11), DRAFT_COLOR, 2);
    draw->AddText(ImVec2(spawn.x + 13, spawn.y - 9), DRAFT_COLOR, "Start");
    if (state.SelectedTransition()) {
        const TransitionEntrance &entrance = PreviewEntrance(state.selection.index);
        if (entrance.reachable) {
            const ImVec2 point = screen(entrance.point);
            draw->AddCircleFilled(point, 8, IM_COL32(26, 23, 15, 245));
            draw->AddCircle(point, 8, SELECTED_COLOR, 0, 2.5f);
            draw->AddCircleFilled(point, 2.5f, SELECTED_COLOR);
            const ImVec2 text(point.x + 12, point.y - 25);
            const ImVec2 size = ImGui::CalcTextSize("Eingang");
            draw->AddRectFilled(ImVec2(text.x - 4, text.y - 3), ImVec2(text.x + size.x + 4, text.y + size.y + 3), IM_COL32(26, 23, 15, 230), 3);
            draw->AddText(text, SELECTED_COLOR, "Eingang");
        }
    }
    if (hovered && inside) {
        const std::string coordinates = std::to_string(int(std::round(pixel.x))) + " / " + std::to_string(int(std::round(pixel.y)));
        draw->AddRectFilled(ImVec2(origin.x + 8, end.y - 31), ImVec2(origin.x + 140, end.y - 7), IM_COL32(12, 20, 29, 220), 4);
        draw->AddText(ImVec2(origin.x + 16, end.y - 27), IM_COL32(220, 235, 245, 255), coordinates.c_str());
    }
    draw->PopClipRect();
}

void NavigationEditor::Shortcuts() {
    const ImGuiIO &io = ImGui::GetIO();
    if (io.WantTextInput || ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId)) return;
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) Save();
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
        if (io.KeyShift) state.Redo(); else state.Undo();
        dragging = false;
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
        state.Redo();
        dragging = false;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) && !state.draft.empty()) FinishDraft();
    if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
        if (state.SelectedProp()) {
            if (dragging) {
                std::string error;
                state.EndDrag(error);
                dragging = false;
            }
            state.DeleteSelectedProp();
            Message("Objekt gelöscht. Mit Strg+Z rückgängig machen.");
        } else if (state.selection.vertex >= 0) {
            std::string error;
            if (!state.DeleteSelectedVertex(error)) Message(error, true);
        }
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        if (!state.draft.empty()) state.CancelDraft();
        else if (dragging) { state.CancelEdit(); dragging = false; }
        else RequestClose();
    }
}

void NavigationEditor::DrawConfirmation() {
    if (confirmRequested) {
        ImGui::OpenPopup("Ungespeicherte Änderungen");
        confirmRequested = false;
    }
    ImGui::SetNextWindowSize(ImVec2(520, 0), ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal("Ungespeicherte Änderungen", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
    ImGui::TextWrapped("%s", pendingQuit
        ? "Die aktuelle Laufkarte wurde geändert. Vor dem Beenden speichern?"
        : "Die aktuelle Laufkarte wurde geändert. Vor dem Wechsel speichern?");
    if (!state.draft.empty())
        ImGui::TextWrapped("Eine Kontur ist noch offen. Zum Speichern zuerst abbrechen und die Kontur abschließen.");
    if (ImGui::Button(pendingQuit ? "Speichern und beenden" : "Speichern und wechseln")) {
        if (Save()) {
            const auto action = std::move(pendingAction);
            pendingAction = {};
            ImGui::CloseCurrentPopup();
            if (action) action();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Verwerfen")) {
        const auto action = std::move(pendingAction);
        pendingAction = {};
        ImGui::CloseCurrentPopup();
        if (action) action();
    }
    ImGui::SameLine();
    if (ImGui::Button("Abbrechen")) {
        pendingAction = {};
        pendingQuit = false;
        ImGui::CloseCurrentPopup();
    }
    if (statusError) ImGui::TextWrapped("%s", status.c_str());
    ImGui::EndPopup();
}

void NavigationEditor::SetDirectory(const fs::path &directory) {
    std::error_code error;
    const fs::path absolute = fs::absolute(directory, error);
    if (error || !fs::is_directory(absolute, error)) {
        Message("Dieser Ordner konnte nicht geöffnet werden.", true);
        return;
    }
    browserDirectory = absolute.lexically_normal();
    browserSelection.clear();
    SetBuffer(browserPath, browserDirectory.string());
}

void NavigationEditor::DrawBrowser() {
    if (browserRequested) {
        ImGui::OpenPopup("Hintergrund importieren");
        browserRequested = false;
    }
    ImGui::SetNextWindowSize(ImVec2(680, 520), ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal("Hintergrund importieren", nullptr, ImGuiWindowFlags_NoCollapse)) return;
    ImGui::TextWrapped("PNG-, JPG-, BMP- oder TGA-Datei auswählen. Das Originalbild bleibt erhalten.");
    ImGui::SetNextItemWidth(-110);
    const bool submitted = ImGui::InputText("##Dateipfad", browserPath.data(), browserPath.size(), ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    const bool openPath = ImGui::Button("Öffnen", ImVec2(100, 0));
    if (submitted || openPath) {
        const fs::path typed(browserPath.data());
        std::error_code error;
        if (fs::is_directory(typed, error)) SetDirectory(typed);
        else if (fs::is_regular_file(typed, error) && BackgroundImageExtension(typed)) browserSelection = typed;
        else Message("Bitte einen vorhandenen Ordner oder eine PNG-, JPG-, BMP- oder TGA-Datei wählen.", true);
    }
    if (ImGui::Button("Übergeordneter Ordner")) SetDirectory(browserDirectory.parent_path());
    ImGui::SameLine();
    ImGui::TextDisabled("%s", browserDirectory.filename().string().c_str());

    fs::path nextDirectory;
    bool chooseFile = false;
    if (ImGui::BeginChild("Dateien", ImVec2(0, -90), ImGuiChildFlags_Borders)) {
        std::vector<fs::directory_entry> entries;
        std::error_code error;
        fs::directory_iterator iterator(browserDirectory, fs::directory_options::skip_permission_denied, error);
        const fs::directory_iterator end;
        while (iterator != end && !error) {
            const fs::directory_entry &entry = *iterator;
            std::error_code typeError;
            if (entry.is_directory(typeError) || (entry.is_regular_file(typeError) && BackgroundImageExtension(entry.path())))
                entries.push_back(entry);
            iterator.increment(error);
        }
        std::sort(entries.begin(), entries.end(), [](const fs::directory_entry &a, const fs::directory_entry &b) {
            std::error_code errorA, errorB;
            const bool directoryA = a.is_directory(errorA), directoryB = b.is_directory(errorB);
            return directoryA != directoryB ? directoryA : a.path().filename() < b.path().filename();
        });
        if (error) ImGui::TextWrapped("Der Ordner ist nicht lesbar.");
        for (const fs::directory_entry &entry : entries) {
            std::error_code typeError;
            const bool directory = entry.is_directory(typeError);
            const std::string label = (directory ? "[Ordner]  " : "") + entry.path().filename().string();
            if (ImGui::Selectable(label.c_str(), browserSelection == entry.path(), ImGuiSelectableFlags_AllowDoubleClick)) {
                if (directory) {
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) nextDirectory = entry.path();
                } else {
                    browserSelection = entry.path();
                    SetBuffer(browserPath, browserSelection.string());
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) chooseFile = true;
                }
            }
        }
    }
    ImGui::EndChild();
    if (!nextDirectory.empty()) SetDirectory(nextDirectory);
    ImGui::TextWrapped("%s", browserSelection.empty() ? "Noch keine Datei ausgewählt." : browserSelection.filename().string().c_str());
    ImGui::BeginDisabled(browserSelection.empty());
    if (ImGui::Button("Importieren", ImVec2(130, 0))) chooseFile = true;
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Abbrechen", ImVec2(130, 0))) ImGui::CloseCurrentPopup();
    if (chooseFile && !browserSelection.empty()) {
        const std::string path = browserSelection.string();
        ImGui::CloseCurrentPopup();
        Import(path);
    }
    if (statusError) ImGui::TextWrapped("%s", status.c_str());
    ImGui::EndPopup();
}

void NavigationEditor::Draw() {
    if (!open) return;
    if (!initialized) {
        ImGui::Begin("Hintergrund öffnen");
        ImGui::TextWrapped("%s", status.c_str());
        if (ImGui::Button("Zurück zum Spiel")) open = false;
        ImGui::End();
        return;
    }
    PushEditorStyle();
    ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::Begin("Laufbereich bearbeiten", nullptr, flags);
    ImGui::TextUnformatted("LAUFBEREICH BEARBEITEN");
    ImGui::SameLine();
    ImGui::TextWrapped("Aktuelle Szene: %s", SceneName(sceneLabel).c_str());
    ImGui::TextDisabled("Grün: Rand   Rot: Hindernis   Lila: Szenenwechsel   Blau: Start   Orange: 3D-Objekt");
    ImGui::Separator();
    const float contentHeight = std::max(100.0f, ImGui::GetContentRegionAvail().y - 68.0f);
    if (ImGui::BeginChild("Werkzeuge", ImVec2(300, contentHeight), ImGuiChildFlags_None)) DrawPanel();
    ImGui::EndChild();
    ImGui::SameLine();
    if (ImGui::BeginChild("Bild", ImVec2(0, contentHeight), ImGuiChildFlags_None,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) DrawCanvas();
    ImGui::EndChild();
    ImGui::Separator();
    if (ImGui::Button("Speichern Strg+S", ImVec2(0, 29))) Save();
    ImGui::SameLine();
    if (ImGui::Button("Im Spiel testen N", ImVec2(0, 29))) RequestClose();
    ImGui::SameLine();
    ColoredText(statusError ? STATUS_ERROR_TEXT : STATUS_TEXT, status);
    ImGui::TextDisabled("%s  -  %s", HasChanges() ? "Nicht gespeichert" : "Unverändert",
                        level.Documents().PathFor(state.document.background).string().c_str());
    Shortcuts();
    DrawBrowser();
    DrawConfirmation();
    ImGui::End();
    PopEditorStyle();
}

void NavigationEditor::Open() {
    level.StopWalking();
    OpenDocument(level.Document(), level.CurrentSceneId());
}

void NavigationEditor::ImportDroppedFile(const std::string &path) {
    open = true;
    Import(path);
}
