#ifndef WALKANIMATION_H
#define WALKANIMATION_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

#include "skeleton.h"

class WalkAnimation {
public:
    void Update(float deltaTime, bool laeuft, bool rennen) {
        zeit += deltaTime;
        naehern(gewicht, laeuft ? 1.0f : 0.0f, deltaTime / UEBERBLENDEN);
        naehern(rennAnteil, rennen ? 1.0f : 0.0f, deltaTime / UMSCHALTEN);
        Gangart bewegung = mischen(GEHEN, RENNEN, Glatt(rennAnteil));
        aktuell = mischen(STEHEN, bewegung, Glatt(gewicht));

        if (gewicht > 0.0f)
            phase = std::fmod(phase + ZWEI_PI * aktuell.zyklenProSekunde * deltaTime, ZWEI_PI);
        else
            phase = 0.0f;
    }

    float Speed() const {
        return 4.0f * BEINLAENGE * std::sin(glm::radians(aktuell.huefte)) * aktuell.zyklenProSekunde;
    }

    void ApplyTo(Pose &pose, float rechterArm = 1.0f) const {
        float s = std::sin(phase);
        float ruhe = 1.0f - Glatt(gewicht);
        float atmen = std::sin(ATEM_TAKT * zeit) * ruhe;

        float hueftLinks = aktuell.huefte * s;
        float hueftRechts = -aktuell.huefte * s;
        float knieLinks = aktuell.knieRuhe + aktuell.knie * buckel(phase + KNIE_VORLAUF);
        float knieRechts = aktuell.knieRuhe + aktuell.knie * buckel(phase + PI + KNIE_VORLAUF);

        float fussLinks = FUSS_AUSGLEICH * (hueftLinks - knieLinks);
        float fussRechts = FUSS_AUSGLEICH * (hueftRechts - knieRechts);

        pose["left leg_07"] = DrehungX(-hueftLinks);
        pose["right leg_010"] = DrehungX(-hueftRechts);
        pose["left knee_08"] = DrehungX(knieLinks);
        pose["right knee_011"] = DrehungX(knieRechts);
        pose["left ankle_09"] = DrehungX(fussLinks);
        pose["right ankle_012"] = DrehungX(fussRechts);

        float pendel = std::sin(phase - ARM_VERZUG);
        float armLinks = -aktuell.arm * pendel;
        float armRechts = aktuell.arm * pendel * rechterArm;
        float senken = aktuell.armSenken + ATEM_ARM * atmen;
        pose["shoulder L_015"] = DrehungX(-armLinks) * DrehungZ(-senken);
        pose["shoulder R_021"] = DrehungX(-armRechts) * DrehungZ(senken * rechterArm);

        float beugeLinks = aktuell.ellbogen + aktuell.ellbogenPendeln * (0.5f - 0.5f * pendel);
        float beugeRechts = (aktuell.ellbogen + aktuell.ellbogenPendeln * (0.5f + 0.5f * pendel)) * rechterArm;
        pose["elbow L_00"] = DrehungY(-beugeLinks);
        pose["elbow R_022"] = DrehungY(beugeRechts);

        float standbein = -std::cos(phase - KOERPER_VERZUG);
        float drehen = std::sin(phase - KOERPER_VERZUG);
        float nicken = aktuell.nicken * std::cos(2.0f * phase - KOERPER_VERZUG);
        pose["upper body_013"] = DrehungX(aktuell.vorbeugen + nicken + ATEM_NICKEN * atmen)
                               * DrehungZ(-aktuell.schwanken * standbein)
                               * DrehungY(aktuell.drehen * drehen);

        float wippen = aktuell.wippen * 0.5f * (std::cos(2.0f * phase) - 1.0f) + ATEM_HEBEN * atmen;
        float seite = aktuell.seite * standbein;
        pose["groove_04"] = glm::translate(glm::mat4(1.0f), glm::vec3(seite, wippen, 0.0f));
    }

private:
    struct Gangart {
        float zyklenProSekunde;
        float huefte;
        float knie;
        float knieRuhe;
        float arm;
        float armSenken;
        float ellbogen;
        float ellbogenPendeln;
        float schwanken;
        float drehen;
        float nicken;
        float wippen;
        float seite;
        float vorbeugen;
    };

    static constexpr Gangart STEHEN = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 87.0f, 20.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.00f, 0.00f, 0.0f};
    static constexpr Gangart GEHEN = {1.45f, 28.0f, 40.0f, 6.0f, 25.0f, 78.0f, 35.0f, 25.0f, 4.0f, 6.0f, 1.5f, 0.25f, 0.12f, 0.0f};
    static constexpr Gangart RENNEN = {2.1f, 38.0f, 75.0f, 12.0f, 40.0f, 80.0f, 85.0f, 0.0f, 5.0f, 10.0f, 3.0f, 0.50f, 0.15f, -10.0f};

    static constexpr float PI = 3.14159265359f;
    static constexpr float ZWEI_PI = 2.0f * PI;
    static constexpr float BEINLAENGE = 2.35f;
    static constexpr float UEBERBLENDEN = 0.3f;
    static constexpr float UMSCHALTEN = 0.4f;

    static constexpr float KNIE_VORLAUF = 0.4f;
    static constexpr float ARM_VERZUG = 0.3f;
    static constexpr float KOERPER_VERZUG = 0.35f;
    static constexpr float FUSS_AUSGLEICH = 0.6f;

    static constexpr float ATEM_TAKT = ZWEI_PI * 0.3f;
    static constexpr float ATEM_HEBEN = 0.05f;
    static constexpr float ATEM_ARM = 1.5f;
    static constexpr float ATEM_NICKEN = 0.7f;

    float zeit = 0.0f;
    float gewicht = 0.0f;
    float rennAnteil = 0.0f;
    float phase = 0.0f;
    Gangart aktuell = STEHEN;

    static Gangart mischen(const Gangart &a, const Gangart &b, float t) {
        return {
            glm::mix(a.zyklenProSekunde, b.zyklenProSekunde, t),
            glm::mix(a.huefte, b.huefte, t),
            glm::mix(a.knie, b.knie, t),
            glm::mix(a.knieRuhe, b.knieRuhe, t),
            glm::mix(a.arm, b.arm, t),
            glm::mix(a.armSenken, b.armSenken, t),
            glm::mix(a.ellbogen, b.ellbogen, t),
            glm::mix(a.ellbogenPendeln, b.ellbogenPendeln, t),
            glm::mix(a.schwanken, b.schwanken, t),
            glm::mix(a.drehen, b.drehen, t),
            glm::mix(a.nicken, b.nicken, t),
            glm::mix(a.wippen, b.wippen, t),
            glm::mix(a.seite, b.seite, t),
            glm::mix(a.vorbeugen, b.vorbeugen, t),
        };
    }

    static void naehern(float &wert, float ziel, float schritt) {
        if (wert < ziel)
            wert = std::min(ziel, wert + schritt);
        else
            wert = std::max(ziel, wert - schritt);
    }

    static float buckel(float x) {
        float h = 0.5f + 0.5f * std::cos(x);
        return h * h;
    }
};

#endif
