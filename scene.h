#ifndef SCENE_H
#define SCENE_H

#include <glm/glm.hpp>

#include <algorithm>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include "character.h"
#include "propplacement.h"
#include "scenecontext.h"
#include "scenetriggers.h"
#include "transitionentrance.h"
#include "walkarea.h"

const char *const SPONGEBOB_MODEL = "chatacters/spongebob/spongebob.glb";
const glm::vec3 SPONGEBOB_PROPORTIONS(1.12f, 0.90f, 1.0f);

class Scene {
public:
    explicit Scene(SceneContext &context)
        : context(context),
          spongebob(context.resources.GetModel(SPONGEBOB_MODEL), glm::vec3(0.0f), 1.0f, SPONGEBOB_PROPORTIONS) {
    }

    bool Enter(const WalkDocument &next, float viewAspect, std::string &error) {
        aspect = viewAspect;
        return ApplyDocument(next, error);
    }

    bool ApplyDocument(const WalkDocument &next, std::string &error) {
        if (!ValidateWalkDocument(next, error)) return false;
        const auto texture = context.resources.GetTextureInfo(next.background);
        if (!texture.id) {
            error = "Der Hintergrund konnte nicht geladen werden.";
            return false;
        }
        if (next.area.imageSize != glm::vec2(texture.width, texture.height)) {
            error = "Die gespeicherte Bildgroesse passt nicht zum Hintergrund. Bitte die Karte im N-Editor anpassen und erneut speichern.";
            return false;
        }
        for (const SceneProp &prop : next.props)
            if (!context.resources.ModelLoaded(prop.model))
                std::cout << "WARNUNG: 3D-Objekt '" << prop.name << "' wird nicht gezeichnet, Modell fehlt: assets/models/"
                          << prop.model << std::endl;

        const glm::vec3 spawn = next.area.ToGround(next.spawn, next.camera, aspect);
        Polygon boundary = next.area.ToGround(next.area.boundary, next.camera, aspect);
        std::vector<Polygon> obstacles;
        for (const Polygon &polygon : next.area.obstacles)
            obstacles.push_back(next.area.ToGround(polygon, next.camera, aspect));
        const float clearance = next.area.clearance * std::max(1.0f, next.spongebobSize);
        Navigation ground;
        ground.Build(boundary, obstacles, next.area.cellSize, clearance);
        if (ground.Empty() || !ground.Contains({spawn.x, spawn.z})) {
            error = "Am Startpunkt fehlt Platz fuer die Figur im aktuellen Fensterformat. Verkleinere die Figur, verbreitere den Laufbereich oder versetze den Startpunkt.";
            return false;
        }
        std::vector<TransitionEntrance> entrances;
        for (const SceneTransition &transition : next.transitions) {
            const TransitionEntrance entrance = ResolveTransitionEntrance(
                next.area, next.camera, aspect, ground, {spawn.x, spawn.z}, transition);
            if (!entrance.reachable) {
                error = "Der Szenenwechsel '" + transition.name +
                    "' ist vom Startpunkt aus nicht erreichbar. Pruefe den Weg zur Tuer und ihre Position am Aussenrand.";
                return false;
            }
            entrances.push_back(entrance);
        }

        document = next;
        backgroundTexture = texture.id;
        navigation = std::move(ground);
        navigationBoundary = std::move(boundary);
        navigationObstacles = std::move(obstacles);
        entrancePoints = std::move(entrances);
        activeTransitions = document.transitions;
        for (std::size_t i = 0; i < document.transitions.size(); ++i)
            if (entrancePoints[i].automatic)
                activeTransitions.push_back(EntranceTrigger(document.transitions[i], entrancePoints[i], document.area.imageSize));
        plannedPath.clear();
        pendingTransition.clear();
        spongebob.SetSize(document.spongebobSize);
        spongebob.PlaceAt(spawn);
        triggers.Reset(activeTransitions, document.spawn);
        error.clear();
        return true;
    }

    void Update(float deltaTime) {
        spongebob.Update(deltaTime);
        if (!pendingTransition.empty()) return;
        std::vector<glm::vec2> trace;
        for (glm::vec3 point : spongebob.MovementTrace())
            trace.push_back(document.area.ToImage(point, document.camera, aspect));
        const int entered = triggers.Advance(activeTransitions, trace);
        if (entered >= 0) {
            pendingTransition = activeTransitions[entered].targetScene;
            spongebob.StopMoving();
        }
    }

    std::string TakeTransition() {
        std::string result;
        result.swap(pendingTransition);
        return result;
    }

    void Draw() {
        context.background.Draw(backgroundTexture);
        const glm::mat4 projection = document.camera.Projection(aspect);
        const glm::mat4 view = document.camera.View();
        Shader &shader = context.characterShader;
        shader.use();
        shader.setMat4("projection", projection);
        shader.setMat4("view", view);
        spongebob.Draw(shader);
        DrawProps(shader);
        if (context.wegeAnzeigen)
            context.grid.DrawNavigation(projection, view, navigationBoundary, navigationObstacles, plannedPath);
    }

    void ClickAt(float mouseX, float mouseY, int width, int height, bool run) {
        if (width <= 0 || height <= 0) return;
        const WalkArea &area = document.area;
        const glm::vec2 pixel(mouseX * area.imageSize.x / width, mouseY * area.imageSize.y / height);
        glm::vec3 goal = area.ToGround(area.ClampToBoundary(pixel), document.camera, aspect);
        const glm::vec3 &start = spongebob.Position();
        std::function<bool(glm::vec2)> acceptsGoal;
        const int door = TransitionAt(pixel);
        if (door >= 0) {
            if (entrancePoints[door].automatic) {
                goal = area.ToGround(entrancePoints[door].point, document.camera, aspect);
            } else {
                acceptsGoal = [this, door](glm::vec2 p) {
                    return PointInPolygon(document.area.ToImage({p.x, 0.0f, p.y}, document.camera, aspect),
                                          document.transitions[door].polygon);
                };
            }
        }
        std::vector<glm::vec3> waypoints;
        for (glm::vec2 point : navigation.FindPath({start.x, start.z}, {goal.x, goal.z}, acceptsGoal))
            waypoints.emplace_back(point.x, 0.0f, point.y);
        plannedPath = {start};
        plannedPath.insert(plannedPath.end(), waypoints.begin(), waypoints.end());
        spongebob.FollowPath(waypoints, run);
    }

    void RaiseHand() { spongebob.RaiseHand(); }
    void StopWalking() { spongebob.StopMoving(); }

    bool HasTransitionAt(float x, float y, int width, int height) const {
        if (width <= 0 || height <= 0) return false;
        const glm::vec2 pixel(x * document.area.imageSize.x / width, y * document.area.imageSize.y / height);
        return TransitionAt(pixel) >= 0;
    }

    const WalkDocument &Document() const { return document; }

private:
    int TransitionAt(glm::vec2 pixel) const {
        for (int i = int(document.transitions.size()) - 1; i >= 0; --i) {
            const SceneTransition &transition = document.transitions[i];
            if (!transition.targetScene.empty() && PointInPolygon(pixel, transition.polygon)) return i;
        }
        return -1;
    }

    void DrawProps(Shader &shader) {
        for (const SceneProp &prop : document.props) {
            Model &model = context.resources.GetModel(prop.model);
            if (!model.Loaded()) continue;
            glm::vec3 minimum = model.minBounds, maximum = model.maxBounds;
            if (!ValidPropBounds(minimum, maximum)) DefaultPropBounds(minimum, maximum);
            const glm::vec3 ground = document.area.ToGround(prop.foot, document.camera, aspect);
            shader.setMat4("model", PropTransform(minimum, maximum, ground, prop.height, prop.rotation));
            const bool skinned = model.skeleton.HasBones();
            shader.setBool("skinned", skinned);
            if (skinned) {
                model.skeleton.ComputeBoneMatrices(Pose(), propBones);
                shader.setMat4Array("bones", propBones);
            }
            model.Draw(shader);
        }
    }

    SceneContext &context;
    WalkDocument document;
    float aspect = 16.0f / 9.0f;
    Character spongebob;
    unsigned int backgroundTexture = 0;
    Navigation navigation;
    Polygon navigationBoundary;
    std::vector<Polygon> navigationObstacles;
    std::vector<glm::vec3> plannedPath;
    std::vector<TransitionEntrance> entrancePoints;
    std::vector<SceneTransition> activeTransitions;
    SceneTriggers triggers;
    std::string pendingTransition;
    std::vector<glm::mat4> propBones;
};

#endif
