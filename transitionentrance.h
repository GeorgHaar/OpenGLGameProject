#ifndef TRANSITIONENTRANCE_H
#define TRANSITIONENTRANCE_H

#include "scenetriggers.h"

#include <cmath>

struct TransitionEntrance {
    bool reachable = false;
    bool automatic = false;
    glm::vec2 point{0.0f};
};

inline TransitionEntrance ResolveTransitionEntrance(
    const WalkArea &area, const Camera &camera, float aspect,
    const Navigation &navigation, glm::vec2 start, const SceneTransition &transition) {
    if (navigation.Empty() || transition.polygon.size() < 3 || !navigation.Contains(start)) return {};
    const auto pixel = [&](glm::vec2 ground) {
        return area.ToImage({ground.x, 0.0f, ground.y}, camera, aspect);
    };
    const auto strict = [&](glm::vec2 p) { return PointInPolygon(pixel(p), transition.polygon); };
    if (strict(start)) return {true, false, pixel(start)};

    glm::vec2 minimum = transition.polygon.front(), maximum = minimum;
    for (glm::vec2 p : transition.polygon) {
        minimum = glm::min(minimum, p);
        maximum = glm::max(maximum, p);
    }
    const glm::vec2 doorstep((minimum.x + maximum.x) * 0.5f, maximum.y);
    const glm::vec3 ground = area.ToGround(area.ClampToBoundary(doorstep), camera, aspect);
    const glm::vec2 goal(ground.x, ground.z);
    auto route = navigation.FindPath(start, goal, strict);
    if (!route.empty()) return {true, false, pixel(route.back())};
    if (navigation.HasWalkablePoint(strict)) return {};

    const float allowance = area.imageSize.y * 0.05f;
    const auto nearby = [&](glm::vec2 p) {
        return PolygonDistance(pixel(p), transition.polygon) <= allowance;
    };
    route = navigation.FindPath(start, goal, nearby);
    if (!route.empty()) return {true, true, pixel(route.back())};
    if (nearby(start) && glm::length(start - goal) < 1e-5f)
        return {true, true, pixel(start)};
    return {};
}

inline SceneTransition EntranceTrigger(const SceneTransition &transition,
                                       const TransitionEntrance &entrance, glm::vec2 imageSize) {
    SceneTransition trigger = transition;
    trigger.polygon.clear();
    const float radius = imageSize.y * 0.005f;
    for (int i = 0; i < 16; ++i) {
        const float angle = static_cast<float>(i) * 6.28318530718f / 16.0f;
        trigger.polygon.push_back(entrance.point + radius * glm::vec2(std::cos(angle), std::sin(angle)));
    }
    return trigger;
}

#endif
