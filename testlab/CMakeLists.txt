cmake_minimum_required(VERSION 3.20)
project(Key4PlayTestLab VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(SDL2
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG release-2.30.11
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(SDL2)

FetchContent_Declare(imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.91.9b
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(imgui)

set(IMGUI_DIR "${imgui_SOURCE_DIR}")
add_library(imgui STATIC
  "${IMGUI_DIR}/imgui.cpp"
  "${IMGUI_DIR}/imgui_draw.cpp"
  "${IMGUI_DIR}/imgui_tables.cpp"
  "${IMGUI_DIR}/imgui_widgets.cpp"
  "${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp"
  "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp")
target_include_directories(imgui PUBLIC "${IMGUI_DIR}" "${IMGUI_DIR}/backends")
target_link_libraries(imgui PUBLIC SDL2::SDL2-static)

find_package(OpenGL REQUIRED)

add_library(key4play_input_core STATIC
  ../shared/src/input_core.c
  ../shared/src/hid_report.c
  ../shared/src/input_snapshot.c
  ../shared/src/pad_jump_plan.c)
target_include_directories(key4play_input_core PUBLIC ../shared/include)
if(NOT MSVC)
  target_link_libraries(key4play_input_core PUBLIC m)
endif()

add_executable(key4play_testlab src/main.cpp)
target_link_libraries(key4play_testlab PRIVATE
  key4play_input_core imgui SDL2::SDL2main SDL2::SDL2-static OpenGL::GL)

include(CTest)
if(BUILD_TESTING)
  add_executable(key4play_core_tests ../shared/tests/input_core_tests.c)
  target_link_libraries(key4play_core_tests PRIVATE key4play_input_core)
  add_test(NAME key4play_core_tests COMMAND key4play_core_tests)
endif()
