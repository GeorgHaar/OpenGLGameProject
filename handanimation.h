#ifndef HANDANIMATION_H
#define HANDANIMATION_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "skeleton.h"

class HandAnimation {
public:
    void Start() {
        if (!laeuft) {
            laeuft = true;
            zeit = 0.0f;
        }
    }

    void Update(float deltaTime) {
        if (!laeuft)
            return;

        zeit += deltaTime;
        if (zeit >= HEBEN + HALTEN + SENKEN) {
            laeuft = false;
            zeit = 0.0f;
        }
    }

    void ApplyTo(Pose &pose) const {
        if (!laeuft)
            return;

        float a = anteil();
        AddToPose(pose, SCHULTER, DrehungZ(-a * SCHULTER_WINKEL));
        AddToPose(pose, ELLBOGEN, DrehungZ(-a * ELLBOGEN_WINKEL));
    }

    float Anteil() const {
        return laeuft ? anteil() : 0.0f;
    }

private:
    static constexpr const char *SCHULTER = "shoulder R_021";
    static constexpr const char *ELLBOGEN = "elbow R_022";

    static constexpr float SCHULTER_WINKEL = 40.0f;
    static constexpr float ELLBOGEN_WINKEL = 50.0f;

    static constexpr float HEBEN = 0.35f;
    static constexpr float HALTEN = 0.4f;
    static constexpr float SENKEN = 0.35f;

    bool laeuft = false;
    float zeit = 0.0f;

    float anteil() const {
        if (zeit < HEBEN)
            return Glatt(zeit / HEBEN);
        if (zeit < HEBEN + HALTEN)
            return 1.0f;
        return 1.0f - Glatt((zeit - HEBEN - HALTEN) / SENKEN);
    }
};

#endif
