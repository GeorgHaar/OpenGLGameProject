#ifndef BACKGROUND_H
#define BACKGROUND_H

#include <GL/glew.h>

#include <string>

#include "shader.h"

class Background {
public:
    Background(const std::string &assetPath)
        : shader((assetPath + "assets/shaders/background.vert").c_str(),
                 (assetPath + "assets/shaders/background.frag").c_str()) {
        setupQuad();
    }

    void Draw(unsigned int texture) {
        glDisable(GL_DEPTH_TEST);

        shader.use();
        shader.setInt("background", 0);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);

        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);

        glEnable(GL_DEPTH_TEST);
    }

private:
    Shader shader;
    unsigned int VAO, VBO;

    void setupQuad() {
        float quadVertices[] = {
            -1.0f, -1.0f, 0.0f, 0.0f,
            1.0f, -1.0f, 1.0f, 0.0f,
            1.0f, 1.0f, 1.0f, 1.0f,

            -1.0f, -1.0f, 0.0f, 0.0f,
            1.0f, 1.0f, 1.0f, 1.0f,
            -1.0f, 1.0f, 0.0f, 1.0f};

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);

        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));

        glBindVertexArray(0);
    }
};

#endif
