
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