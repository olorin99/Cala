add_library(stb_image STATIC stb_image.cpp stb_image.h)

add_library(stb_image_write STATIC stb_image_write.cpp stb_image_write.h)

add_subdirectory(Ende)
add_subdirectory(fastgltf)
add_subdirectory(volk)

set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(SPIRV-Headers)

set(SPIRV_WARN_EVERYTHING OFF CACHE BOOL "" FORCE)
set(SPIRV_SKIP_TESTS ON CACHE BOOL "" FORCE)
set(SKIP_SPIRV_TOOLS_INSTALL ON CACHE BOOL "" FORCE)
add_subdirectory(SPIRV-Tools)

set(SKIP_GLSLANG_INSTALL ON CACHE BOOL "" FORCE)
set(ENABLE_GLSLANG_JS OFF CACHE BOOL "" FORCE)
add_subdirectory(glslang)

set(SPIRV_CROSS_EXCEPTIONS_TO_ASSERTIONS ON CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_STATIC ON CACHE BOOL "" FORCE)
add_subdirectory(SPIRV-Cross)

set(SHADERC_SKIP_TESTS ON CACHE BOOL "" FORCE)
set(SHADERC_SKIP_EXAMPLES ON CACHE BOOL "" FORCE)
add_subdirectory(shaderc)
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(shaderc PUBLIC -Wno-everything -std=c++20)
    target_compile_options(shaderc_util PUBLIC -Wno-everything -std=c++20)
endif()


set(IMGUI_PATH "imgui")
#file(GLOB IMGUI_SOURCES ${IMGUI_PATH}/*.cpp)
set(IMGUI_SOURCES ${IMGUI_PATH}/imgui.cpp ${IMGUI_PATH}/imgui_demo.cpp ${IMGUI_PATH}/imgui_tables.cpp ${IMGUI_PATH}/imgui_widgets.cpp ${IMGUI_PATH}/imgui_draw.cpp ${IMGUI_PATH}/backends/imgui_impl_vulkan.cpp ${IMGUI_PATH}/backends/imgui_impl_sdl2.cpp )
add_library(imgui STATIC ${IMGUI_SOURCES})
target_include_directories(imgui PUBLIC ${IMGUI_PATH})

add_library(node_editor STATIC node_editor/imgui_canvas.cpp node_editor/imgui_node_editor.cpp node_editor/imgui_node_editor_api.cpp node_editor/crude_json.cpp)
target_include_directories(node_editor PUBLIC node_editor ${IMGUI_PATH})

add_library(implot STATIC implot/implot.h implot/implot.cpp implot/implot_internal.h implot/implot_items.cpp)
target_include_directories(implot PUBLIC implot ${IMGUI_PATH})