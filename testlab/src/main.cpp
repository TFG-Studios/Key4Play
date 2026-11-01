#include "key4play/input_core.hpp"

#include <SDL.h>
#include <SDL_opengl.h>
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

#include <algorithm>
#include <cstdio>
#include <string>

namespace {
void draw_stick(ImDrawList* draw, ImVec2 center, float radius,
                key4play::Stick value, const char* label) {
    draw->AddCircle(center, radius, IM_COL32(115, 130, 155, 255), 48, 2.0F);
    draw->AddCircle(center, radius * 0.55F, IM_COL32(75, 90, 115, 160), 48, 1.0F);
    draw->AddLine({center.x - radius, center.y}, {center.x + radius, center.y},
                  IM_COL32(75, 90, 115, 160));
    draw->AddLine({center.x, center.y - radius}, {center.x, center.y + radius},
                  IM_COL32(75, 90, 115, 160));
    const ImVec2 dot{center.x + value.x * radius, center.y + value.y * radius};
    draw->AddCircleFilled(dot, 8.0F, IM_COL32(65, 190, 255, 255));
    draw->AddText({center.x - 24.0F, center.y + radius + 12.0F},
                  IM_COL32(220, 225, 235, 255), label);
}
}

int main(int, char**) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS) != 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_Window* window = SDL_CreateWindow(
        "Key4Play Test Lab",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1180, 780, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 9.0F;
    style.FrameRounding = 6.0F;
    style.GrabRounding = 6.0F;
    style.WindowPadding = ImVec2(16, 14);

    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 130");

    key4play::Settings settings;
    key4play::InputFrame frame;
    bool capture_mouse = false;
    bool running = true;
    float mouse_dx = 0.0F, mouse_dy = 0.0F;
    std::string status = "Simulation only — no controller output is sent";
    unsigned long long frames = 0;

    while (running) {
        SDL_Event event;
        mouse_dx = mouse_dy = 0.0F;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) running = false;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                if (capture_mouse) {
                    capture_mouse = false;
                    SDL_SetRelativeMouseMode(SDL_FALSE);
                    status = "Mouse released";
                } else {
                    running = false;
                }
            }
            if (event.type == SDL_MOUSEMOTION && capture_mouse) {
                mouse_dx += static_cast<float>(event.motion.xrel);
                mouse_dy += static_cast<float>(event.motion.yrel);
            }
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        const bool gui_wants_keyboard = ImGui::GetIO().WantCaptureKeyboard;
        frame = key4play::make_frame(
            !gui_wants_keyboard && keys[SDL_SCANCODE_W],
            !gui_wants_keyboard && keys[SDL_SCANCODE_S],
            !gui_wants_keyboard && keys[SDL_SCANCODE_A],
            !gui_wants_keyboard && keys[SDL_SCANCODE_D],
            mouse_dx, mouse_dy, settings);
        ++frames;

        ImGui::SetNextWindowPos({0, 0}, ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);
        ImGui::Begin("Key4Play Test Lab", nullptr,
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar);

        if (ImGui::BeginMenuBar()) {
            ImGui::TextUnformatted("KEY4PLAY");
            ImGui::SameLine();
            ImGui::TextDisabled(" / INPUT ENGINE WORKBENCH");
            ImGui::EndMenuBar();
        }

        ImGui::TextWrapped("Test keyboard-to-stick and mouse-to-stick processing before touching the PS4.");
        ImGui::Spacing();
        ImGui::Separator();

        if (ImGui::BeginTabBar("MainTabs")) {
            if (ImGui::BeginTabItem("Live Controller")) {
                ImGui::TextColored({0.35F, 0.78F, 1.0F, 1.0F}, "LIVE INPUT PREVIEW");
                ImGui::Text("WASD = left stick    |    Mouse = right stick");
                ImGui::TextDisabled("ESC releases captured mouse; press ESC again to close.");
                ImGui::Spacing();

                const ImVec2 area = ImGui::GetContentRegionAvail();
                ImDrawList* draw = ImGui::GetWindowDrawList();
                const ImVec2 origin = ImGui::GetCursorScreenPos();
                const float radius = std::min(82.0F, area.y * 0.19F);
                const ImVec2 left_center{origin.x + area.x * 0.27F, origin.y + radius + 20.0F};
                const ImVec2 right_center{origin.x + area.x * 0.72F, origin.y + radius + 20.0F};
                draw_stick(draw, left_center, radius, frame.left, "LEFT STICK");
                draw_stick(draw, right_center, radius, frame.right, "RIGHT STICK");
                ImGui::Dummy({area.x, radius * 2.0F + 65.0F});

                ImGui::Separator();
                ImGui::Text("Movement: X %+.3f   Y %+.3f", frame.left.x, frame.left.y);
                ImGui::Text("Look:      X %+.3f   Y %+.3f", frame.right.x, frame.right.y);
                ImGui::Text("Mouse delta: %.0f, %.0f", mouse_dx, mouse_dy);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Mouse Tuning")) {
                ImGui::TextWrapped("Tune the mouse conversion. These values affect the preview only.");
                ImGui::SliderFloat("Sensitivity", &settings.mouse_sensitivity, 0.001F, 0.08F, "%.3f");
                ImGui::SliderFloat("Deadzone", &settings.deadzone, 0.0F, 0.35F, "%.3f");
                ImGui::SliderFloat("Acceleration", &settings.acceleration, 0.0F, 2.0F, "%.2f");
                ImGui::Checkbox("Capture mouse for raw movement test", &capture_mouse);
                SDL_SetRelativeMouseMode(capture_mouse ? SDL_TRUE : SDL_FALSE);
                if (capture_mouse) {
                    ImGui::TextColored({1.0F, 0.75F, 0.25F, 1.0F}, "Mouse captured — press ESC to release.");
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Keyboard")) {
                ImGui::Text("Current movement bindings");
                ImGui::BulletText("W — forward");
                ImGui::BulletText("A — left");
                ImGui::BulletText("S — backward");
                ImGui::BulletText("D — right");
                ImGui::Separator();
                ImGui::Text("Pressed keys: %s%s%s%s",
                    keys[SDL_SCANCODE_W] ? "W " : "",
                    keys[SDL_SCANCODE_A] ? "A " : "",
                    keys[SDL_SCANCODE_S] ? "S " : "",
                    keys[SDL_SCANCODE_D] ? "D " : "");
                ImGui::TextDisabled("Custom key rebinding is planned for a later iteration.");
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Diagnostics")) {
                ImGui::Text("Frames rendered: %llu", frames);
                ImGui::Text("SDL video driver: %s", SDL_GetCurrentVideoDriver());
                ImGui::Text("Mouse capture: %s", capture_mouse ? "ON" : "OFF");
                ImGui::Text("Input core: portable C++ module");
                ImGui::Separator();
                ImGui::TextWrapped("%s", status.c_str());
                ImGui::TextColored({1.0F, 0.72F, 0.25F, 1.0F},
                    "No virtual gamepad, game injection, or PS4 connection is implemented.");
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        ImGui::End();
        ImGui::Render();

        int display_w = 0, display_h = 0;
        SDL_GL_GetDrawableSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.055F, 0.07F, 0.10F, 1.0F);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    SDL_SetRelativeMouseMode(SDL_FALSE);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}