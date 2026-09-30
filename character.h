#ifndef CHARACTER_H
#define CHARACTER_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <cstddef>
#include <vector>

#include "handanimation.h"
#include "model.h"
#include "shader.h"
#include "skeleton.h"
#include "walkanimation.h"

class Character {
public:
    Character(Model &model, const glm::vec3 &position, float groesse,
              const glm::vec3 &proportionen = glm::vec3(1.0f))
        : model(&model), position(position), groesse(groesse), proportionen(proportionen) {
    }

    const glm::vec3 &Position() const {
        return position;
    }

    void SetSize(float groesse) {
        this->groesse = groesse;
    }

    float Scale() const {
        return groesse / model->Height();
    }

    const std::vector<glm::vec3> &MovementTrace() const { return movementTrace; }

    void PlaceAt(const glm::vec3 &point) {
        position = point;
        StopMoving();
    }

    void FollowPath(const std::vector<glm::vec3> &points, bool rennen) {
        wegpunkte = points;
        for (glm::vec3 &punkt : wegpunkte)
            punkt.y = position.y;
        naechsterWegpunkt = 0;
        this->rennen = !wegpunkte.empty() && rennen;
    }

    void StopMoving() {
        wegpunkte.clear();
        naechsterWegpunkt = 0;
        rennen = false;
    }

    void RaiseHand() {
        handAnimation.Start();
    }

    void Update(float deltaTime) {
        movementTrace.clear();
        movementTrace.push_back(position);
        float schritt = walkAnimation.Speed() * Scale() * proportionen.z * deltaTime;
        while (naechsterWegpunkt < wegpunkte.size()) {
            const glm::vec3 &ziel = wegpunkte[naechsterWegpunkt];
            glm::vec3 weg = ziel - position;
            float abstand = glm::length(weg);
            if (abstand > 0.0f) {
                glm::vec3 richtung = weg / abstand;
                zielDrehung = glm::degrees(std::atan2(richtung.x, richtung.z));
                if (abstand > schritt) {
                    position += richtung * schritt;
                    if (movementTrace.back() != position) movementTrace.push_back(position);
                    break;
                }
            }

            position = ziel;
            if (movementTrace.back() != position) movementTrace.push_back(position);
            schritt -= abstand;
            ++naechsterWegpunkt;
        }
        bool laeuft = naechsterWegpunkt < wegpunkte.size();
        if (!laeuft)
            StopMoving();

        float differenz = std::fmod(zielDrehung - drehung + 540.0f, 360.0f) - 180.0f;
        drehung = std::fmod(drehung + differenz * (1.0f - std::exp(-DREH_TEMPO * deltaTime)), 360.0f);

        walkAnimation.Update(deltaTime, laeuft, rennen);
        handAnimation.Update(deltaTime);
    }

    void Draw(Shader &shader) {
        glm::mat4 transform = glm::mat4(1.0f);
        transform = glm::translate(transform, position);
        transform = glm::rotate(transform, glm::radians(drehung), glm::vec3(0.0f, 1.0f, 0.0f));
        transform = glm::scale(transform, Scale() * proportionen);
        transform = glm::translate(transform, glm::vec3(0.0f, -model->FloorY(), 0.0f));

        shader.setMat4("model", transform);

        bool skinned = model->skeleton.HasBones();
        shader.setBool("skinned", skinned);
        if (skinned) {
            Pose pose;
            pose["wrist L_016"] = glm::scale(glm::mat4(1.0f), glm::vec3(HAND_GROESSE));
            pose["wrist R_023"] = glm::scale(glm::mat4(1.0f), glm::vec3(HAND_GROESSE));

            walkAnimation.ApplyTo(pose, 1.0f - handAnimation.Anteil());
            handAnimation.ApplyTo(pose);
            model->skeleton.ComputeBoneMatrices(pose, boneMatrices);
            shader.setMat4Array("bones", boneMatrices);
        }

        model->Draw(shader);
    }

private:
    static constexpr float DREH_TEMPO = 7.0f;
    static constexpr float HAND_GROESSE = 0.7f;

    Model *model;
    glm::vec3 position;
    float groesse;
    glm::vec3 proportionen;
    float drehung = 0.0f;
    float zielDrehung = 0.0f;

    std::vector<glm::vec3> wegpunkte;
    std::vector<glm::vec3> movementTrace;
    std::size_t naechsterWegpunkt = 0;
    bool rennen = false;

    WalkAnimation walkAnimation;
    HandAnimation handAnimation;
    std::vector<glm::mat4> boneMatrices;
};

#endif
