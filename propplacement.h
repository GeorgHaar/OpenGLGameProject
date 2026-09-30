#ifndef PROPPLACEMENT_H
#define PROPPLACEMENT_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <cmath>

inline void DefaultPropBounds(glm::vec3 &minimum, glm::vec3 &maximum) {
    minimum = glm::vec3(-0.5f, 0.0f, -0.5f);
    maximum = glm::vec3(0.5f, 1.0f, 0.5f);
}

inline bool ValidPropBounds(const glm::vec3 &minimum, const glm::vec3 &maximum) {
    for (int axis = 0; axis < 3; ++axis)
        if (!std::isfinite(minimum[axis]) || !std::isfinite(maximum[axis])) return false;
    return maximum.y > minimum.y;
}


inline float PropScale(const glm::vec3 &minimum, const glm::vec3 &maximum, float height) {
    const float modelHeight = maximum.y > minimum.y ? maximum.y - minimum.y : 1.0f;
    return height / modelHeight;
}

inline glm::mat4 PropTransform(const glm::vec3 &minimum, const glm::vec3 &maximum,
                               const glm::vec3 &ground, float height, float rotationDegrees) {
    glm::mat4 transform = glm::translate(glm::mat4(1.0f), ground);
    transform = glm::rotate(transform, glm::radians(rotationDegrees), glm::vec3(0.0f, 1.0f, 0.0f));
    transform = glm::scale(transform, glm::vec3(PropScale(minimum, maximum, height)));
    const glm::vec3 anchor((minimum.x + maximum.x) * 0.5f, minimum.y, (minimum.z + maximum.z) * 0.5f);
    return glm::translate(transform, -anchor);
}


inline std::array<glm::vec3, 8> PropBoxCorners(const glm::vec3 &minimum, const glm::vec3 &maximum,
                                               const glm::mat4 &transform) {
    std::array<glm::vec3, 8> corners;
    const float xs[4] = {minimum.x, maximum.x, maximum.x, minimum.x};
    const float zs[4] = {minimum.z, minimum.z, maximum.z, maximum.z};
    for (int i = 0; i < 4; ++i) {
        corners[i] = glm::vec3(transform * glm::vec4(xs[i], minimum.y, zs[i], 1.0f));
        corners[i + 4] = glm::vec3(transform * glm::vec4(xs[i], maximum.y, zs[i], 1.0f));
    }
    return corners;
}

#endif
