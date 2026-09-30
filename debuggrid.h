#ifndef DEBUGGRID_H
#define DEBUGGRID_H

#include <GL/glew.h>
#include <glm/glm.hpp>

#include <cmath>
#include <string>
#include <vector>

#include "shader.h"

class DebugGrid {
public:
    DebugGrid(const std::string &assetPath)
        : shader((assetPath + "assets/shaders/grid.vert").c_str(),
                 (assetPath + "assets/shaders/grid.frag").c_str()) {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);

        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void *)0);
        glBindVertexArray(0);
    }

    void DrawNavigation(const glm::mat4 &projection, const glm::mat4 &view,
                        const std::vector<glm::vec2> &boundary,
                        const std::vector<std::vector<glm::vec2>> &obstacles,
                        const std::vector<glm::vec3> &path) {
        shader.use();
        shader.setMat4("projection", projection);
        shader.setMat4("view", view);
        const bool hadDepth = glIsEnabled(GL_DEPTH_TEST);
        glDisable(GL_DEPTH_TEST);
        auto outline = [this](const std::vector<glm::vec2> &polygon, const glm::vec4 &color) {
            std::vector<glm::vec3> lines;
            for (std::size_t i = 0; i < polygon.size(); ++i) {
                const glm::vec2 a = polygon[i], b = polygon[(i + 1) % polygon.size()];
                lines.emplace_back(a.x, 0.0f, a.y);
                lines.emplace_back(b.x, 0.0f, b.y);
            }
            zeichne(lines, color);
        };
        outline(boundary, glm::vec4(0.1f, 1.0f, 0.25f, 1.0f));
        for (const auto &polygon : obstacles)
            outline(polygon, glm::vec4(1.0f, 0.15f, 0.1f, 1.0f));
        std::vector<glm::vec3> lines;
        for (std::size_t i = 1; i < path.size(); ++i) {
            lines.push_back(path[i - 1]);
            lines.push_back(path[i]);
        }
        zeichne(lines, glm::vec4(1.0f, 0.9f, 0.0f, 1.0f));
        if (hadDepth) glEnable(GL_DEPTH_TEST);
    }

private:
    Shader shader;
    unsigned int VAO, VBO;

    void zeichne(const std::vector<glm::vec3> &punkte, const glm::vec4 &farbe) {
        if (punkte.empty())
            return;

        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, punkte.size() * sizeof(glm::vec3), punkte.data(), GL_STREAM_DRAW);
        shader.setVec4("farbe", farbe);
        glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(punkte.size()));
        glBindVertexArray(0);
    }
};

#endif
