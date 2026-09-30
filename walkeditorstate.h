#ifndef WALKEDITORSTATE_H
#define WALKEDITORSTATE_H

#include "walkdocument.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

class WalkEditorState {
public:
    enum class Tool { Select, Boundary, Obstacle, Spawn, Transition, Prop };
    enum class PolygonType { None, Boundary, Obstacle, Transition, Prop };
    struct Selection {
        PolygonType type = PolygonType::None;
        int index = -1;
        int vertex = -1;
        int edge = -1;
    };

    WalkDocument document;
    std::vector<glm::vec2> draft;
    Selection selection;
    std::string placementModel;
    float placementHeight = 0.5f;

    void Load(const WalkDocument &value) {
        document = value;
        saved = value;
        undo.clear();
        redo.clear();
        beforeEdit.reset();
        draft.clear();
        ClearSelection();
        tool = Tool::Select;
    }

    bool Dirty() const { return document != saved; }
    void MarkSaved() { saved = document; }
    Tool GetTool() const { return tool; }

    void SetTool(Tool value) {
        if (value == tool) return;
        EndEdit();
        CancelDraft();
        tool = value;
    }

    void CancelDraft() { draft.clear(); }
    void ClearSelection() { selection = Selection{}; }

    void BeginEdit() {
        if (!beforeEdit) beforeEdit = document;
    }

    bool EndEdit() {
        if (!beforeEdit) return false;
        const bool changed = document != *beforeEdit;
        if (changed) {
            undo.push_back(*beforeEdit);
            redo.clear();
        }
        beforeEdit.reset();
        return changed;
    }

    void CancelEdit() {
        if (beforeEdit) document = *beforeEdit;
        beforeEdit.reset();
    }

    bool EndDrag(std::string &error) {
        error.clear();
        const Polygon *polygon = SelectedPolygon();
        if (polygon && !SimplePolygon(*polygon, error)) {
            CancelEdit();
            return false;
        }
        EndEdit();
        return true;
    }

    bool Click(glm::vec2 point, std::string &error) {
        error.clear();
        if (!Finite(point)) {
            error = "Der Punkt muss endliche Bildkoordinaten haben.";
            return false;
        }
        if (tool == Tool::Select)
            return HitVertex(point, 8.0f) || HitEdge(point, 8.0f) || HitPolygon(point);
        if (tool == Tool::Spawn) {
            if (document.spawn == point) return false;
            Remember();
            document.spawn = point;
            return true;
        }
        if (tool == Tool::Prop) return PlaceProp(point, placementModel, placementHeight, error);
        if (!draft.empty() && glm::length(point - draft.back()) < 0.0001f) {
            error = "Zwei aufeinanderfolgende Punkte muessen verschieden sein.";
            return false;
        }
        draft.push_back(point);
        return true;
    }
    bool Click(glm::vec2 point) { std::string error; return Click(point, error); }

    bool FinishPolygon(std::string &error) {
        error.clear();
        if (tool != Tool::Boundary && tool != Tool::Obstacle && tool != Tool::Transition) {
            error = "Waehle zuerst Laufgrenze, Hindernis oder Uebergang.";
            return false;
        }
        if (!SimplePolygon(draft, error)) return false;
        Remember();
        if (tool == Tool::Boundary) {
            document.area.boundary = draft;
            SelectBoundary();
        } else if (tool == Tool::Obstacle) {
            document.area.obstacles.push_back(draft);
            SelectObstacle(document.area.obstacles.size() - 1);
        } else {
            SceneTransition transition;
            transition.name = UniqueName("Übergang", document.transitions);
            transition.polygon = draft;
            document.transitions.push_back(transition);
            SelectTransition(document.transitions.size() - 1);
        }
        draft.clear();
        return true;
    }

    Polygon *SelectedPolygon() {
        switch (selection.type) {
        case PolygonType::Boundary:
            return &document.area.boundary;
        case PolygonType::Obstacle:
            if (selection.index >= 0 && std::size_t(selection.index) < document.area.obstacles.size())
                return &document.area.obstacles[selection.index];
            break;
        case PolygonType::Transition:
            if (SceneTransition *transition = SelectedTransition()) return &transition->polygon;
            break;
        case PolygonType::Prop:
        case PolygonType::None:
            break;
        }
        return nullptr;
    }
    const Polygon *SelectedPolygon() const { return const_cast<WalkEditorState *>(this)->SelectedPolygon(); }

    bool PlaceProp(glm::vec2 foot, const std::string &model, float height, std::string &error) {
        error.clear();
        if (model.empty()) {
            error = "Waehle zuerst ein 3D-Objekt aus der Liste.";
            return false;
        }
        if (!(height > 0.0f)) {
            error = "Die Hoehe muss groesser als 0 sein.";
            return false;
        }
        SceneProp prop;
        prop.model = model;
        prop.foot = foot;
        prop.height = height;
        prop.name = UniqueName(PropStem(model), document.props);
        Remember();
        document.props.push_back(prop);
        SelectProp(document.props.size() - 1);
        return true;
    }

    static std::string PropStem(const std::string &model) {
        std::string stem = model;
        const auto slash = stem.find_last_of('/');
        if (slash != std::string::npos) stem.erase(0, slash + 1);
        const auto dot = stem.find_last_of('.');
        if (dot != std::string::npos && dot > 0) stem.erase(dot);
        return stem.empty() ? "Objekt" : stem;
    }

    bool SelectProp(std::size_t index) {
        if (index >= document.props.size()) { ClearSelection(); return false; }
        selection = {PolygonType::Prop, int(index), -1, -1};
        return true;
    }

    SceneProp *SelectedProp() {
        if (selection.type != PolygonType::Prop || selection.index < 0 ||
            std::size_t(selection.index) >= document.props.size()) return nullptr;
        return &document.props[selection.index];
    }
    const SceneProp *SelectedProp() const { return const_cast<WalkEditorState *>(this)->SelectedProp(); }

    bool HitProp(glm::vec2 point, float radius) {
        ClearSelection();
        if (!Finite(point)) return false;
        double best = double(radius) * radius;
        for (std::size_t i = document.props.size(); i > 0; --i) {
            const double distance = DistanceSquared(document.props[i - 1].foot, point);
            if (distance <= best && (distance < best || selection.type == PolygonType::None)) {
                best = distance;
                selection = {PolygonType::Prop, int(i - 1), -1, -1};
            }
        }
        return selection.type == PolygonType::Prop;
    }

    bool DeleteSelectedProp() {
        if (!SelectedProp()) return false;
        EndEdit();
        Remember();
        document.props.erase(document.props.begin() + selection.index);
        ClearSelection();
        return true;
    }

    bool SelectBoundary() {
        selection = {PolygonType::Boundary, -1, -1, -1};
        return true;
    }

    bool SelectObstacle(std::size_t index) {
        if (index >= document.area.obstacles.size()) { ClearSelection(); return false; }
        selection = {PolygonType::Obstacle, int(index), -1, -1};
        return true;
    }

    bool SelectTransition(std::size_t index) {
        if (index >= document.transitions.size()) { ClearSelection(); return false; }
        selection = {PolygonType::Transition, int(index), -1, -1};
        return true;
    }

    SceneTransition *SelectedTransition() {
        if (selection.type != PolygonType::Transition || selection.index < 0 ||
            std::size_t(selection.index) >= document.transitions.size()) return nullptr;
        return &document.transitions[selection.index];
    }
    const SceneTransition *SelectedTransition() const {
        return const_cast<WalkEditorState *>(this)->SelectedTransition();
    }

    bool HitVertex(glm::vec2 point, float radius) {
        ClearSelection();
        if (!Finite(point)) return false;
        double best = double(radius) * radius;
        EachPolygon([&](PolygonType type, int index, const Polygon &polygon) {
            for (std::size_t i = 0; i < polygon.size(); ++i) {
                const double distance = DistanceSquared(polygon[i], point);
                if (distance <= best) {
                    best = distance;
                    selection = {type, index, int(i), -1};
                }
            }
        });
        return selection.vertex >= 0;
    }

    bool HitEdge(glm::vec2 point, float radius) {
        ClearSelection();
        if (!Finite(point)) return false;
        double best = double(radius) * radius;
        EachPolygon([&](PolygonType type, int index, const Polygon &polygon) {
            for (std::size_t i = 0; i < polygon.size(); ++i) {
                const double distance = PointSegmentDistanceSquared(point, polygon[i], polygon[(i + 1) % polygon.size()]);
                if (distance <= best) {
                    best = distance;
                    selection = {type, index, -1, int(i)};
                }
            }
        });
        return selection.edge >= 0;
    }

    bool HitPolygon(glm::vec2 point) {
        ClearSelection();
        if (!Finite(point)) return false;
        for (std::size_t i = document.transitions.size(); i > 0; --i)
            if (PointInPolygon(point, document.transitions[i - 1].polygon)) return SelectTransition(i - 1);
        for (std::size_t i = document.area.obstacles.size(); i > 0; --i)
            if (PointInPolygon(point, document.area.obstacles[i - 1])) return SelectObstacle(i - 1);
        if (PointInPolygon(point, document.area.boundary)) return SelectBoundary();
        return false;
    }

    bool MoveSelected(glm::vec2 point) {
        if (!Finite(point)) return false;
        if (SceneProp *prop = SelectedProp()) {
            if (prop->foot == point) return false;
            Remember();
            prop->foot = point;
            return true;
        }
        Polygon *polygon = SelectedPolygon();
        if (!polygon || selection.vertex < 0 || std::size_t(selection.vertex) >= polygon->size()) return false;
        if ((*polygon)[selection.vertex] == point) return false;
        if (!beforeEdit) {
            Polygon candidate = *polygon;
            candidate[selection.vertex] = point;
            std::string error;
            if (!SimplePolygon(candidate, error)) return false;
        }
        Remember();
        (*polygon)[selection.vertex] = point;
        return true;
    }

    bool DeleteSelectedVertex(std::string &error) {
        error.clear();
        Polygon *polygon = SelectedPolygon();
        if (!polygon || selection.vertex < 0 || std::size_t(selection.vertex) >= polygon->size()) {
            error = "Waehle zuerst einen Eckpunkt.";
            return false;
        }
        if (polygon->size() <= 3) {
            error = "Eine Kontur braucht mindestens drei Eckpunkte. Entferne bei Bedarf die ganze Kontur.";
            return false;
        }
        Polygon candidate = *polygon;
        candidate.erase(candidate.begin() + selection.vertex);
        if (!SimplePolygon(candidate, error)) return false;
        Remember();
        *polygon = candidate;
        selection.vertex = std::min(selection.vertex, int(polygon->size()) - 1);
        selection.edge = -1;
        return true;
    }

    bool DeleteSelectedPolygon() {
        if (!SelectedPolygon()) return false;
        Remember();
        if (selection.type == PolygonType::Boundary) document.area.boundary.clear();
        else if (selection.type == PolygonType::Obstacle)
            document.area.obstacles.erase(document.area.obstacles.begin() + selection.index);
        else if (selection.type == PolygonType::Transition)
            document.transitions.erase(document.transitions.begin() + selection.index);
        ClearSelection();
        return true;
    }

    bool InsertVertex(glm::vec2 point, std::string &error) {
        error.clear();
        Polygon *polygon = SelectedPolygon();
        const int edge = selection.edge;
        if (!polygon || edge < 0 || std::size_t(edge) >= polygon->size() || !Finite(point)) {
            error = "Waehle zuerst eine Kante.";
            return false;
        }
        const std::size_t next = (std::size_t(edge) + 1) % polygon->size();
        point = ClosestPointOnSegment(point, (*polygon)[edge], (*polygon)[next]);
        Polygon candidate = *polygon;
        candidate.insert(candidate.begin() + edge + 1, point);
        if (!SimplePolygon(candidate, error)) return false;
        Remember();
        *polygon = candidate;
        selection.vertex = edge + 1;
        selection.edge = -1;
        return true;
    }

    bool CanUndo() const { return !undo.empty(); }
    bool CanRedo() const { return !redo.empty(); }
    bool Undo() { return Restore(undo, redo); }
    bool Redo() { return Restore(redo, undo); }

private:
    Tool tool = Tool::Select;
    WalkDocument saved;
    std::optional<WalkDocument> beforeEdit;
    std::vector<WalkDocument> undo, redo;

    void Remember() {
        if (beforeEdit) return;
        undo.push_back(document);
        redo.clear();
    }

    bool Restore(std::vector<WalkDocument> &source, std::vector<WalkDocument> &destination) {
        EndEdit();
        if (source.empty()) return false;
        destination.push_back(document);
        document = source.back();
        source.pop_back();
        CancelDraft();
        ClearSelection();
        return true;
    }

    template <typename List> static std::string UniqueName(const std::string &stem, const List &entries) {
        for (int number = 1;; ++number) {
            const std::string name = stem + " " + std::to_string(number);
            bool taken = false;
            for (const auto &entry : entries) if (entry.name == name) taken = true;
            if (!taken) return name;
        }
    }

    template <typename Function> void EachPolygon(Function fn) const {
        fn(PolygonType::Boundary, -1, document.area.boundary);
        for (std::size_t i = 0; i < document.area.obstacles.size(); ++i)
            fn(PolygonType::Obstacle, int(i), document.area.obstacles[i]);
        for (std::size_t i = 0; i < document.transitions.size(); ++i)
            fn(PolygonType::Transition, int(i), document.transitions[i].polygon);
    }
};

#endif
