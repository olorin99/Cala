add_subdirectory(Editor)

add_subdirectory(Ende)

add_subdirectory(SPIRV-Cross)

set(IMGUI_PATH "imgui")
#file(GLOB IMGUI_SOURCES ${IMGUI_PATH}/*.cpp)
set(IMGUI_SOURCES ${IMGUI_PATH}/imgui.cpp ${IMGUI_PATH}/imgui_demo.cpp ${IMGUI_PATH}/imgui_tables.cpp ${IMGUI_PATH}/imgui_widgets.cpp ${IMGUI_PATH}/imgui_draw.cpp ${IMGUI_PATH}/backends/imgui_impl_vulkan.cpp ${IMGUI_PATH}/backends/imgui_impl_sdl2.cpp )
add_library(imgui STATIC ${IMGUI_SOURCES})
target_include_directories(imgui PUBLIC ${IMGUI_PATH})