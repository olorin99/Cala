
add_executable(hello_triangle hello_triangle.cpp)
target_link_libraries(hello_triangle Cala Ende stb_image)

add_executable(phong phong.cpp)
target_link_libraries(phong Cala Ende stb_image)

add_executable(blinn_phong blinn_phong.cpp)
target_link_libraries(blinn_phong Cala Ende stb_image)

add_executable(vulkan_raw vulkan_raw.cpp)
target_link_libraries(vulkan_raw Cala Ende stb_image)

add_executable(vulkan_deferred vulkan_deferred.cpp)
target_link_libraries(vulkan_deferred Cala Ende stb_image)

add_executable(instanced instanced.cpp)
target_link_libraries(instanced Cala Ende stb_image)

add_executable(culling culling.cpp)
target_link_libraries(culling Cala Ende stb_image)

add_executable(gpuCulling gpuCulling.cpp)
target_link_libraries(gpuCulling Cala Ende stb_image)

add_executable(indirect_draw indirect_draw.cpp)
target_link_libraries(indirect_draw Cala Ende stb_image)

add_executable(point_shadows point_shadows.cpp)
target_link_libraries(point_shadows Cala Ende stb_image)

add_executable(direct_shadows direct_shadows.cpp)
target_link_libraries(direct_shadows Cala Ende stb_image)
