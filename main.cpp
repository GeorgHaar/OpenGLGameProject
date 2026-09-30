#include <stb_image.h>

#include <GL/glew.h>
#include <SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>

#include <iostream>
#include <string>

#include "level01.h"
#include "music.h"
#include "navigationeditor.h"

const unsigned int SCR_WIDTH = 1920;
const unsigned int SCR_HEIGHT = 1080;

namespace {
void HandleGameInput(SDL_Window *window, Level01 &level, NavigationEditor &editor, const SDL_Event &event) {
    if (event.type == SDL_KEYDOWN) {
        if (event.key.keysym.sym == SDLK_ESCAPE)
            editor.RequestQuit();
        if (event.key.keysym.sym == SDLK_v)
            level.ToggleNavigation();
        if (event.key.keysym.sym == SDLK_m && !event.key.repeat)
            music::ToggleMute();
        if (event.key.keysym.sym == SDLK_PLUS || event.key.keysym.sym == SDLK_EQUALS ||
            event.key.keysym.sym == SDLK_KP_PLUS)
            music::ChangeVolume(0.1f);
        if (event.key.keysym.sym == SDLK_MINUS || event.key.keysym.sym == SDLK_KP_MINUS)
            music::ChangeVolume(-0.1f);
    }

    if (event.type == SDL_MOUSEBUTTONDOWN) {
        if (event.button.button == SDL_BUTTON_LEFT) {
            int fensterBreite, fensterHoehe;
            SDL_GetWindowSize(window, &fensterBreite, &fensterHoehe);
            level.ClickAt(event.button.x, event.button.y, fensterBreite, fensterHoehe,
                          event.button.clicks >= 2);
        }
        if (event.button.button == SDL_BUTTON_RIGHT)
            level.RaiseHand();
    }
}

void RenderFrame(SDL_Window *window, Level01 &level) {
    int width, height;
    SDL_GL_GetDrawableSize(window, &width, &height);
    glViewport(0, 0, width, height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    level.Draw(width, height);
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    SDL_GL_SwapWindow(window);
}

bool RunGame(SDL_Window *window, const std::string &assets) {
    int width, height;
    SDL_GL_GetDrawableSize(window, &width, &height);
    Level01 level(assets, static_cast<float>(width) / height);
    if (!level.Ready()) return false;
    NavigationEditor editor(level);

    Uint64 letzteZeit = SDL_GetTicks64();
    while (!editor.ShouldQuit()) {
        Uint64 jetzt = SDL_GetTicks64();
        float deltaTime = (jetzt - letzteZeit) / 1000.0f;
        letzteZeit = jetzt;

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                editor.RequestQuit();
            if (event.type == SDL_DROPFILE) {
                const std::string file = event.drop.file ? event.drop.file : "";
                SDL_free(event.drop.file);
                if (!editor.IsOpen()) editor.Open();
                editor.ImportDroppedFile(file);
                continue;
            }
            if (event.type == SDL_KEYDOWN && !event.key.repeat &&
                event.key.keysym.sym == SDLK_n && !ImGui::GetIO().WantTextInput) {
                if (editor.IsOpen()) editor.RequestClose();
                else editor.Open();
                continue;
            }
            if (editor.IsOpen()) continue;
            HandleGameInput(window, level, editor, event);
        }
        music::Update();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        if (editor.IsOpen()) {
            editor.Draw();
        } else {
            level.Update(deltaTime);
            int mouseX, mouseY, windowWidth, windowHeight;
            SDL_GetMouseState(&mouseX, &mouseY);
            SDL_GetWindowSize(window, &windowWidth, &windowHeight);
            if (level.HasTransitionAt(mouseX, mouseY, windowWidth, windowHeight))
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }
        RenderFrame(window, level);
    }
    return true;
}
}

int main(int, char *[]) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cout << "SDL konnte nicht initialisiert werden: " << SDL_GetError() << std::endl;
        return -1;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_Window *window = SDL_CreateWindow("SpongebobGame", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          SCR_WIDTH, SCR_HEIGHT, SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN_DESKTOP);
    if (!window) {
        std::cout << "Fenster konnte nicht erstellt werden: " << SDL_GetError() << std::endl;
        return -1;
    }
    SDL_GLContext context = SDL_GL_CreateContext(window);
    if (!context) {
        std::cout << "OpenGL-Kontext konnte nicht erstellt werden: " << SDL_GetError() << std::endl;
        return -1;
    }
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cout << "GLEW konnte nicht initialisiert werden" << std::endl;
        return -1;
    }
    std::cout << "OpenGL " << glGetString(GL_VERSION) << std::endl;
    std::cout << "Renderer: " << glGetString(GL_RENDERER) << std::endl;
    SDL_GL_SetSwapInterval(1);
    stbi_set_flip_vertically_on_load(true);

    const std::string assets = SPONGEBOB_PROJECT_DIR;

    music::Start(assets + "assets/music");

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::StyleColorsDark();
    ImGui::GetStyle().ScaleAllSizes(1.15f);
    ImGui::GetStyle().FontScaleMain = 1.3f;
    ImGui_ImplSDL2_InitForOpenGL(window, context);
    ImGui_ImplOpenGL3_Init("#version 460 core");

    const bool started = RunGame(window, assets);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    music::Stop();
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return started ? 0 : 1;
}
