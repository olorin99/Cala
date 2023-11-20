add_subdirectory(Ende)

add_subdirectory(SPIRV-Cross)

add_subdirectory(volk)

add_subdirectory(fastgltf)

set(IMGUI_PATH "imgui")
#file(GLOB IMGUI_SOURCES ${IMGUI_PATH}/*.cpp)
set(IMGUI_SOURCES ${IMGUI_PATH}/imgui.cpp ${IMGUI_PATH}/imgui_demo.cpp ${IMGUI_PATH}/imgui_tables.cpp ${IMGUI_PATH}/imgui_widgets.cpp ${IMGUI_PATH}/imgui_draw.cpp ${IMGUI_PATH}/backends/imgui_impl_vulkan.cpp ${IMGUI_PATH}/backends/imgui_impl_sdl2.cpp )
add_library(imgui STATIC ${IMGUI_SOURCES})
target_include_directories(imgui PUBLIC ${IMGUI_PATH})

add_library(node_editor STATIC node_editor/imgui_canvas.cpp node_editor/imgui_node_editor.cpp node_editor/imgui_node_editor_api.cpp node_editor/crude_json.cpp)
target_include_directories(node_editor PUBLIC node_editor ${IMGUI_PATH})

add_library(implot STATIC implot/implot.h implot/implot.cpp implot/implot_internal.h implot/implot_items.cpp)
target_include_directories(implot PUBLIC implot ${IMGUI_PATH})