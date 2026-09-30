#ifndef CAMERA_H
#define CAMERA_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>

struct Camera {
    float horizont = 0.5f;
    float augenhoehe = 1.5f;
    float sichtfeld = 45.0f;

    glm::mat4 View() const {
        return glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -augenhoehe, 0.0f));
    }

    glm::mat4 Projection(float seitenverhaeltnis) const {
        float halb = NAH * std::tan(glm::radians(sichtfeld / 2.0f));
        float unten = -2.0f * halb * horizont;
        float oben = 2.0f * halb * (1.0f - horizont);
        float breit = halb * seitenverhaeltnis;
        return glm::frustum(-breit, breit, unten, oben, NAH, FERN);
    }

    static constexpr float NAH = 0.1f;
    static constexpr float FERN = 200.0f;
};

inline bool operator==(const Camera &a, const Camera &b) {
    return a.horizont == b.horizont && a.augenhoehe == b.augenhoehe && a.sichtfeld == b.sichtfeld;
}
inline bool operator!=(const Camera &a, const Camera &b) { return !(a == b); }

#endif
