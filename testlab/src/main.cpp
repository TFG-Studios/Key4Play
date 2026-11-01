#include "key4play/input_core.h"

#include <SDL.h>
#include <SDL_opengl.h>
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace {
void draw_stick(ImDrawList* draw, ImVec2 c, float r, uint8_t x, uint8_t y,
                const char* label) {
    draw->AddCircle(c, r, IM_COL32(120,145,175,255), 48, 2.0F);
    draw->AddCircle(c, r*0.55F, IM_COL32(75,90,115,150), 48, 1.0F);
    draw->AddLine({c.x-r,c.y},{c.x+r,c.y},IM_COL32(75,90,115,150));
    draw->AddLine({c.x,c.y-r},{c.x,c.y+r},IM_COL32(75,90,115,150));
    ImVec2 dot{c.x+(static_cast<float>(x)-128.0F)/127.0F*r,
               c.y+(static_cast<float>(y)-128.0F)/127.0F*r};
    draw->AddCircleFilled(dot,8,IM_COL32(55,190,255,255));
    draw->AddText({c.x-34,c.y+r+8},IM_COL32(220,230,240,255),label);
}
void draw_button(ImDrawList* d, ImVec2 c, const char* label, bool down) {
    ImU32 fill=down?IM_COL32(55,175,235,255):IM_COL32(47,59,78,255);
    ImU32 text=down?IM_COL32(255,255,255,255):IM_COL32(185,200,220,255);
    d->AddCircleFilled(c,15,fill);
    d->AddCircle(c,15,IM_COL32(130,150,180,255),24,1.2F);
    ImVec2 size=ImGui::CalcTextSize(label);
    d->AddText({c.x-size.x/2,c.y-size.y/2},text,label);
}
void draw_controller(const K4pPadState& s) {
    ImDrawList* d=ImGui::GetWindowDrawList();
    ImVec2 p=ImGui::GetCursorScreenPos(), size=ImGui::GetContentRegionAvail();
    float scale=std::min(1.0F,std::max(0.65F,size.y/350.0F));
    float w=600*scale,h=250*scale;
    ImVec2 o{p.x+(size.x-w)/2,p.y+8};
    d->AddRectFilled({o.x+65*scale,o.y+60*scale},{o.x+535*scale,o.y+205*scale},IM_COL32(40,49,65,255),75*scale);
    d->AddRect({o.x+65*scale,o.y+60*scale},{o.x+535*scale,o.y+205*scale},IM_COL32(100,120,150,255),75*scale,0,2);
    draw_stick(d,{o.x+190*scale,o.y+137*scale},39*scale,s.lx,s.ly,"LEFT STICK");
    draw_stick(d,{o.x+410*scale,o.y+137*scale},39*scale,s.rx,s.ry,"RIGHT STICK");
    draw_button(d,{o.x+445*scale,o.y+94*scale},"△",(s.buttons&K4P_BTN_TRIANGLE)!=0);
    draw_button(d,{o.x+480*scale,o.y+129*scale},"○",(s.buttons&K4P_BTN_CIRCLE)!=0);
    draw_button(d,{o.x+410*scale,o.y+129*scale},"□",(s.buttons&K4P_BTN_SQUARE)!=0);
    draw_button(d,{o.x+445*scale,o.y+164*scale},"×",(s.buttons&K4P_BTN_CROSS)!=0);
    draw_button(d,{o.x+118*scale,o.y+112*scale},"↑",(s.buttons&K4P_BTN_UP)!=0);
    draw_button(d,{o.x+118*scale,o.y+162*scale},"↓",(s.buttons&K4P_BTN_DOWN)!=0);
    draw_button(d,{o.x+93*scale,o.y+137*scale},"←",(s.buttons&K4P_BTN_LEFT)!=0);
    draw_button(d,{o.x+143*scale,o.y+137*scale},"→",(s.buttons&K4P_BTN_RIGHT)!=0);
    draw_button(d,{o.x+270*scale,o.y+103*scale},"SH",(s.buttons&K4P_BTN_SHARE)!=0);
    draw_button(d,{o.x+330*scale,o.y+103*scale},"OP",(s.buttons&K4P_BTN_OPTIONS)!=0);
    draw_button(d,{o.x+185*scale,o.y+67*scale},"L1",(s.buttons&K4P_BTN_L1)!=0);
    draw_button(d,{o.x+415*scale,o.y+67*scale},"R1",(s.buttons&K4P_BTN_R1)!=0);
    draw_button(d,{o.x+300*scale,o.y+85*scale},"TP",(s.buttons&K4P_BTN_TOUCHPAD)!=0);
    draw_button(d,{o.x+190*scale,o.y+178*scale},"L3",(s.buttons&K4P_BTN_L3)!=0);
    draw_button(d,{o.x+410*scale,o.y+178*scale},"R3",(s.buttons&K4P_BTN_R3)!=0);
    d->AddRectFilled({o.x+220*scale,o.y+24*scale},{o.x+280*scale,o.y+34*scale},IM_COL32(48,60,78,255),4);
    d->AddRectFilled({o.x+220*scale,o.y+24*scale},{o.x+(220.0F+60.0F*s.l2/255.0F)*scale,o.y+34*scale},IM_COL32(65,180,245,255),4);
    d->AddRectFilled({o.x+320*scale,o.y+24*scale},{o.x+380*scale,o.y+34*scale},IM_COL32(48,60,78,255),4);
    d->AddRectFilled({o.x+320*scale,o.y+24*scale},{o.x+(320.0F+60.0F*s.r2/255.0F)*scale,o.y+34*scale},IM_COL32(65,180,245,255),4);
    ImGui::Dummy({size.x,h+28});
}
void make_reports(const Uint8* keys, uint8_t keyboard_packet[8],
                  uint8_t mouse_packet[4], float mdx, float mdy) {
    std::memset(keyboard_packet,0,8);
    std::memset(mouse_packet,0,4);
    keyboard_packet[0]=(keys[SDL_SCANCODE_LCTRL]?K4P_MOD_LEFT_CTRL:0)|
                       (keys[SDL_SCANCODE_LSHIFT]?K4P_MOD_LEFT_SHIFT:0);
    struct Pair {SDL_Scancode scan;uint8_t usage;};
    const Pair pairs[]={{SDL_SCANCODE_W,0x1a},{SDL_SCANCODE_A,0x04},
      {SDL_SCANCODE_S,0x16},{SDL_SCANCODE_D,0x07},{SDL_SCANCODE_SPACE,0x2c},
      {SDL_SCANCODE_E,0x08},{SDL_SCANCODE_Q,0x14},{SDL_SCANCODE_R,0x15},
      {SDL_SCANCODE_C,0x06},{SDL_SCANCODE_V,0x19},{SDL_SCANCODE_TAB,0x2b},
      {SDL_SCANCODE_RETURN,0x28},{SDL_SCANCODE_UP,0x52},{SDL_SCANCODE_DOWN,0x51},
      {SDL_SCANCODE_LEFT,0x50},{SDL_SCANCODE_RIGHT,0x4f}};
    unsigned count=0;
    for(const auto& pair:pairs)
        if(keys[pair.scan]&&count<6) keyboard_packet[2+count++]=pair.usage;

    Uint32 buttons=SDL_GetMouseState(nullptr,nullptr);
    if(buttons&SDL_BUTTON(SDL_BUTTON_LEFT)) mouse_packet[0]|=1;
    if(buttons&SDL_BUTTON(SDL_BUTTON_RIGHT)) mouse_packet[0]|=2;
    if(buttons&SDL_BUTTON(SDL_BUTTON_MIDDLE)) mouse_packet[0]|=4;
    mouse_packet[1]=static_cast<uint8_t>(static_cast<int8_t>(
        std::max(-127.0f,std::min(127.0f,mdx))));
    mouse_packet[2]=static_cast<uint8_t>(static_cast<int8_t>(
        std::max(-127.0f,std::min(127.0f,mdy))));
}
}

int main(int,char**) {
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER|SDL_INIT_EVENTS)!=0) {
        std::fprintf(stderr,"SDL_Init failed: %s\n",SDL_GetError());return 1;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
    SDL_Window* window=SDL_CreateWindow("Key4Play Test Lab v0.2",
      SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,1024,640,
      SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI);
    if(!window){std::fprintf(stderr,"Window failed: %s\n",SDL_GetError());SDL_Quit();return 1;}
    SDL_SetWindowMinimumSize(window,800,520);
    SDL_GLContext gl=SDL_GL_CreateContext(window);
    if(!gl){std::fprintf(stderr,"OpenGL context failed: %s\n",SDL_GetError());SDL_DestroyWindow(window);SDL_Quit();return 1;}
    SDL_GL_MakeCurrent(window,gl);SDL_GL_SetSwapInterval(1);
    IMGUI_CHECKVERSION();ImGui::CreateContext();ImGui::StyleColorsDark();
    ImGuiStyle& st=ImGui::GetStyle();st.WindowRounding=8;st.FrameRounding=5;st.GrabRounding=5;
    st.WindowPadding={13,11};st.ItemSpacing={8,6};
    ImGui_ImplSDL2_InitForOpenGL(window,gl);ImGui_ImplOpenGL3_Init("#version 130");

    K4pSettings settings;k4p_settings_default(&settings);
    K4pPadState keyboardState,mouseState,output;
    k4p_pad_state_neutral(&keyboardState);k4p_pad_state_neutral(&mouseState);
    bool running=true,capture=false;float dx=0,dy=0;
    unsigned long long frames=0;
    while(running) {
        SDL_Event ev;dx=dy=0;
        while(SDL_PollEvent(&ev)) {
            ImGui_ImplSDL2_ProcessEvent(&ev);
            if(ev.type==SDL_QUIT)running=false;
            if(ev.type==SDL_KEYDOWN&&ev.key.keysym.sym==SDLK_ESCAPE) {
                if(capture){capture=false;SDL_SetRelativeMouseMode(SDL_FALSE);}else running=false;
            }
            if(ev.type==SDL_MOUSEMOTION&&capture){dx+=ev.motion.xrel;dy+=ev.motion.yrel;}
        }
        ImGui_ImplOpenGL3_NewFrame();ImGui_ImplSDL2_NewFrame();ImGui::NewFrame();
        const Uint8* keys=SDL_GetKeyboardState(nullptr);
        uint8_t keyboardPacket[8],mousePacket[4];
        K4pKeyboardReport kr;K4pMouseReport mr;
        make_reports(keys,keyboardPacket,mousePacket,dx,dy);
        if(!ImGui::GetIO().WantCaptureKeyboard &&
           k4p_decode_boot_keyboard(keyboardPacket,sizeof(keyboardPacket),&kr))
            k4p_map_keyboard(&kr,&keyboardState);
        else k4p_pad_state_neutral(&keyboardState);
        if(!capture){mousePacket[1]=0;mousePacket[2]=0;}
        if(k4p_decode_boot_mouse(mousePacket,sizeof(mousePacket),&mr))
            k4p_map_mouse(&mr,&settings,&mouseState);
        else k4p_pad_state_neutral(&mouseState);
        k4p_pad_state_neutral(&output);
        k4p_merge_pad_state(&output,&keyboardState);k4p_merge_pad_state(&output,&mouseState);
        ++frames;

        ImGui::SetNextWindowSize({1024,640},ImGuiCond_FirstUseEver);
        ImGui::Begin("Key4Play Test Lab",nullptr,ImGuiWindowFlags_NoCollapse);
        ImGui::TextColored({0.35f,0.78f,1.0f,1.0f},"KEY4PLAY");
        ImGui::SameLine();ImGui::TextDisabled("INPUT ENGINE WORKBENCH / v0.2");
        ImGui::SameLine();ImGui::SetCursorPosX(ImGui::GetWindowWidth()-230);
        ImGui::TextColored({0.3f,0.85f,0.55f,1.0f},"● SIMULATION MODE");
        ImGui::Separator();
        if(ImGui::BeginTabBar("K4PTabs")) {
            if(ImGui::BeginTabItem("Controller")) {
                ImGui::TextDisabled("Live DualShock-style state preview");draw_controller(output);
                ImGui::Separator();
                ImGui::Text("Buttons: 0x%08X",output.buttons);
                ImGui::Text("LX %3u LY %3u   RX %3u RY %3u",output.lx,output.ly,output.rx,output.ry);
                ImGui::Text("L2 %3u R2 %3u",output.l2,output.r2);ImGui::EndTabItem();
            }
            if(ImGui::BeginTabItem("Bindings")) {
                ImGui::Columns(2,nullptr,false);
                ImGui::Text("W A S D");ImGui::NextColumn();ImGui::Text("Left analog stick");ImGui::NextColumn();
                ImGui::Text("Arrow keys");ImGui::NextColumn();ImGui::Text("D-pad");ImGui::NextColumn();
                ImGui::Text("Space / Left Ctrl");ImGui::NextColumn();ImGui::Text("Cross / Circle");ImGui::NextColumn();
                ImGui::Text("E / Q");ImGui::NextColumn();ImGui::Text("Square / Triangle");ImGui::NextColumn();
                ImGui::Text("Left Shift / R");ImGui::NextColumn();ImGui::Text("L1 / R1");ImGui::NextColumn();
                ImGui::Text("C / V");ImGui::NextColumn();ImGui::Text("L3 / R3");ImGui::NextColumn();
                ImGui::Text("Tab / Enter");ImGui::NextColumn();ImGui::Text("Share / Options");ImGui::NextColumn();
                ImGui::Columns(1);ImGui::Separator();
                ImGui::Text("Mouse: left = R2, right = L2, middle = touchpad");
                ImGui::Text("Mouse movement = right stick (relative capture in Mouse tab)");
                ImGui::TextDisabled("PS button intentionally remains on the physical DS4.");
                ImGui::EndTabItem();
            }
            if(ImGui::BeginTabItem("Mouse")) {
                ImGui::SliderFloat("Sensitivity",&settings.mouse_sensitivity,0.001f,0.08f,"%.3f");
                ImGui::SliderFloat("Deadzone",&settings.mouse_deadzone,0.0f,0.35f,"%.3f");
                ImGui::SliderFloat("Acceleration",&settings.mouse_acceleration,0.0f,2.0f,"%.2f");
                if(ImGui::Checkbox("Capture mouse movement",&capture))SDL_SetRelativeMouseMode(capture?SDL_TRUE:SDL_FALSE);
                if(capture)ImGui::TextColored({1.0f,0.75f,0.25f,1.0f},"Mouse captured — press ESC to release.");
                ImGui::Text("Last frame delta: %.0f, %.0f",dx,dy);
                ImGui::Text("Right stick: %u / %u",output.rx,output.ry);ImGui::EndTabItem();
            }
            if(ImGui::BeginTabItem("Diagnostics")) {
                ImGui::Text("Frames rendered: %llu",frames);
                ImGui::Text("SDL video driver: %s",SDL_GetCurrentVideoDriver());
                ImGui::Text("Mouse capture: %s",capture?"ON":"OFF");
                ImGui::Text("Shared core: C11 / HID boot report decoder + DS4 mapper");
                ImGui::Separator();
                ImGui::TextColored({1.0f,0.72f,0.25f,1.0f},
                  "No virtual gamepad, Windows game injection, or PS4 connection is implemented.");
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::End();ImGui::Render();
        int ww,wh;SDL_GL_GetDrawableSize(window,&ww,&wh);glViewport(0,0,ww,wh);
        glClearColor(0.055f,0.07f,0.10f,1);glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());SDL_GL_SwapWindow(window);
    }
    SDL_SetRelativeMouseMode(SDL_FALSE);
    ImGui_ImplOpenGL3_Shutdown();ImGui_ImplSDL2_Shutdown();ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl);SDL_DestroyWindow(window);SDL_Quit();return 0;
}
