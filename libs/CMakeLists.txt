add_subdirectory(Ende)

add_subdirectory(SPIRV-Cross)

set(IMGUI_PATH "imgui")
#file(GLOB IMGUI_SOURCES ${IMGUI_PATH}/*.cpp)
set(IMGUI_SOURCES ${IMGUI_PATH}/imgui.cpp ${IMGUI_PATH}/imgui_demo.cpp ${IMGUI_PATH}/imgui_tables.cpp ${IMGUI_PATH}/imgui_widgets.cpp ${IMGUI_PATH}/imgui_draw.cpp ${IMGUI_PATH}/backends/imgui_impl_vulkan.cpp ${IMGUI_PATH}/backends/imgui_impl_sdl2.cpp )
add_library(imgui STATIC ${IMGUI_SOURCES})
target_include_directories(imgui PUBLIC ${IMGUI_PATH})

add_library(IMGUI ImGui/Stats.cpp ImGui/Stats.h ImGui/ImGuiWindow.h)
target_include_directories(IMGUI
        PUBLIC
        $<INSTALL_INTERFACE:ImGui>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/ImGui>
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/ImGui
        third_party)
target_link_libraries(IMGUI Ende imgui)