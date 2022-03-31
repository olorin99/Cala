add_subdirectory(Ende)



add_library(IMGUI ImGui/Stats.cpp ImGui/Stats.h ImGui/ImGuiWindow.h)
target_include_directories(IMGUI
        PUBLIC
        $<INSTALL_INTERFACE:ImGui>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/ImGui>
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/ImGui
        third_party)
target_link_libraries(IMGUI Ende imgui)