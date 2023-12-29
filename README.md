# Cala

### Rendering engine written in C++ using the Vulkan API.

Cala is the Quenya word for light so this project is a simulatio of light.

### Features:
- [x] Linux support
- [x] Render graph
- [x] Visibility buffer
- [x] Mesh shaders
- [x] Meshlet frustum culling
- [x] Mesh LOD
- [x] Direct and point lights
- [x] Clusterd lights
- [x] Cascading shadow maps for direct light
- [x] PCSS (Percentage Closer Soft Shadows) for direct light
- [x] PCF (Percentage Closer Filtering) for point light
- [x] Shader reflection and hot reloading
- [x] Multiple tonemaping functions available (AGX, ACES, Reinhard, ...)

### Build
```
git clone --recursive https://github.com/olorin99/Cala.git
cd Cala
mkdir build
cd build
cmake ..
make
```
A static library and an executable called main should be built. To use simply link the static library or run the executable to test.
The executable supports drag and drop for gltf files. So can simply drag gltf files into the window to view them using Cala.


### Images
#### Meshlet View of Sponza
![cala_meshlet_sponza.png](docs%2Fcala_meshlet_sponza.png)
#### Primitive View of Sponza
![cala_primitive_sponza.png](docs%2Fcala_primitive_sponza.png)
#### Primitive View showing differences between LODs
![cala_primitive_lod_dragons.png](docs%2Fcala_primitive_lod_dragons.png)
#### View of Sponza using point light and AGX tonemap
![cala_agx_sponza.png](docs%2Fcala_agx_sponza.png)

### Dependencies
##### system library
- SDL2 - windowing system

##### git submodules
- meshoptimizer - Meshlet and LOD generation
- spirv-cross - spirv reflection
- spirv-headers
- spirv-tools
- glslang
- shaderc - glsl compilation
- Ende - some common functionality
- fastgltf - gltf loader
- imgui - gui
- implot - nice plots for gui
- node_editor - create graphs in gui

##### source included
- stb_image - image loader
- vma - vulkan memory allocator
- nlohmann json - json parser
- tsl - robin hash map