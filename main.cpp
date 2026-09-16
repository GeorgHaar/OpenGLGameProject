#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <string>
#include <SDL.h>
#include <SDL_opengl.h>

#include <iostream>

int main(int, char*[]) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL konnte nicht initialisiert werden: " << SDL_GetError() << '\n';
        SDL_Quit();
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "SpongebobGame",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN_DESKTOP
    );

    if (!window) {
        std::cerr << "Fenster konnte nicht erstellt werden: " << SDL_GetError() << '\n';
        SDL_Quit();
        return 1;
    }

    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    if (!glContext) {
        std::cerr << "OpenGL-Kontext konnte nicht erstellt werden: " << SDL_GetError() << '\n';
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

        char* basePath = SDL_GetBasePath();
        if (!basePath) {
            std::cerr << "Programmpfad konnte nicht ermittelt werden: "
                    << SDL_GetError() << '\n';
            SDL_GL_DeleteContext(glContext);
            SDL_DestroyWindow(window);
            SDL_Quit();
            return 1;
        }

        const std::string imagePath =
            std::string(basePath) + "assets/chapter2_tvStation_background00.png";
        SDL_free(basePath);

        stbi_set_flip_vertically_on_load(true);

        int imageWidth = 0;
        int imageHeight = 0;
        int imageChannels = 0;

        unsigned char* imageData = stbi_load(
            imagePath.c_str(),
            &imageWidth,
            &imageHeight,
            &imageChannels,
            STBI_rgb_alpha
        );

        if (!imageData) {
            std::cerr << "Hintergrundbild konnte nicht geladen werden: "
                    << stbi_failure_reason() << '\n';
            SDL_GL_DeleteContext(glContext);
            SDL_DestroyWindow(window);
            SDL_Quit();
            return 1;
        }

        GLuint backgroundTexture = 0;
        glGenTextures(1, &backgroundTexture);
        glBindTexture(GL_TEXTURE_2D, backgroundTexture);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGBA,
            imageWidth,
            imageHeight,
            0,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            imageData
        );

        glBindTexture(GL_TEXTURE_2D, 0);
        stbi_image_free(imageData);

        std::cout << "Hintergrundtextur erstellt: "
                << imageWidth << " x " << imageHeight << '\n';


    const bool vsync = SDL_GL_SetSwapInterval(1) == 0;
    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT ||
                (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) {
                running = false;
            }
        }
        if (!running) {
            break;
        }

      int drawableWidth = 0;
    int drawableHeight = 0;
    SDL_GL_GetDrawableSize(window, &drawableWidth, &drawableHeight);

    glViewport(0, 0, drawableWidth, drawableHeight);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, backgroundTexture);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    glBegin(GL_QUADS);

    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(-1.0f, -1.0f);

    glTexCoord2f(1.0f, 0.0f);
    glVertex2f(1.0f, -1.0f);

    glTexCoord2f(1.0f, 1.0f);
    glVertex2f(1.0f, 1.0f);

    glTexCoord2f(0.0f, 1.0f);
    glVertex2f(-1.0f, 1.0f);

    glEnd();

    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);

    SDL_GL_SwapWindow(window);

    if (!vsync) {
        SDL_Delay(16);
    }
}

    glDeleteTextures(1, &backgroundTexture);
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
