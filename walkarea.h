#ifndef WALKAREA_H
#define WALKAREA_H

#include <glm/glm.hpp>

#include <cmath>
#include <vector>

#include "geometry.h"
#include "navigation.h"
#include "camera.h"

struct WalkArea {
    glm::vec2 imageSize{1920.0f, 1080.0f};
    Polygon boundary;
    std::vector<Polygon> obstacles;
    float clearance = 0.42f;
    float cellSize = 0.18f;

    bool Enabled() const { return boundary.size() >= 3; }

    glm::vec3 ToGround(glm::vec2 pixel, const Camera &camera, float aspect) const {
        const float belowHorizon = std::max(0.001f,
            pixel.y / imageSize.y - (1.0f - camera.horizont));
        const float tangent = std::tan(glm::radians(camera.sichtfeld * 0.5f));
        const float distance = camera.augenhoehe / (2.0f * tangent * belowHorizon);
        const float x = (2.0f * pixel.x / imageSize.x - 1.0f) * distance * tangent * aspect;
        return glm::vec3(x, 0.0f, -distance);
    }

    glm::vec2 ToImage(const glm::vec3 &ground, const Camera &camera, float aspect) const {
        const glm::vec4 clip = camera.Projection(aspect) * camera.View() * glm::vec4(ground, 1.0f);
        return glm::vec2((clip.x / clip.w + 1.0f) * 0.5f * imageSize.x,
                         (1.0f - clip.y / clip.w) * 0.5f * imageSize.y);
    }

    Polygon ToGround(const Polygon &pixels, const Camera &camera, float aspect) const {
        Polygon result;
        for (glm::vec2 pixel : pixels) {
            const glm::vec3 p = ToGround(pixel, camera, aspect);
            result.emplace_back(p.x, p.z);
        }
        return result;
    }

    glm::vec2 ClampToBoundary(glm::vec2 pixel) const {
        if (!Enabled() || PointInPolygon(pixel, boundary)) return pixel;
        return ClosestPointOnOutline(pixel, boundary);
    }
};

inline bool operator==(const WalkArea &a, const WalkArea &b) {
    return a.imageSize == b.imageSize && a.boundary == b.boundary && a.obstacles == b.obstacles &&
        a.clearance == b.clearance && a.cellSize == b.cellSize;
}
inline bool operator!=(const WalkArea &a, const WalkArea &b) { return !(a == b); }

#endif
