
add_library(glad STATIC glad.c)
if (UNIX)
    target_link_libraries(glad dl)
endif()

add_library(stb_image STATIC stb_image.cpp stb_image.h)

set(IMGUI_PATH "imgui")
file(GLOB IMGUI_SOURCES ${IMGUI_PATH}/*.cpp)
add_library(imgui STATIC ${IMGUI_SOURCES})
target_include_directories(imgui PUBLIC ${IMGUI_PATH})