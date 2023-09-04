
layout (location = 0) in VsOut {
    vec3 FragPos;
    vec2 TexCoords;
    mat3 TBN;
    vec3 ViewPos;
    flat uint drawID;
} fsIn;

layout (location = 0) out vec4 FragColour;

layout (set = 0, binding = 0) uniform samplerCube cubeMaps[];
layout (set = 0, binding = 0) uniform sampler2D textureMaps[];

struct CameraData {
    mat4 projection;
    mat4 view;
    vec3 position;
    float near;
    float far;
    float exposure;
};

layout (set = 1, binding = 0) uniform FrameData {
    CameraData camera;
};

layout (push_constant) uniform IBLData {
    uvec4 tileSizes;
    uvec2 screenSize;
    int irradianceIndex;
    int prefilteredIndex;
    int brdfIndex;
};

#include "util.glsl"
#include "pbr.glsl"
#include "shadow.glsl"
#include "lighting.glsl"

struct LightGrid {
    uint offset;
    uint count;
};

layout (set = 2, binding = 2) buffer LightGridSSBO {
    LightGrid lightGrid[];
};

layout (set = 2, binding = 3) buffer LightIndices {
    uint globalLightIndices[];
};

layout (set = 3, binding = 0) readonly buffer LightData {
    Light lights[];
};

MATERIAL_DATA;

MATERIAL_DEFINITION;

MATERIAL_LOAD;

MATERIAL_EVAL;

layout (set = 2, binding = 0) readonly buffer MatData {
    MaterialData materials[];
};

struct Mesh {
    uint firstIndex;
    uint indexCount;
    uint materialIndex;
    uint materialInstanceIndex;
    vec4 min;
    vec4 max;
};

layout (set = 2, binding = 1) readonly buffer MeshData {
    Mesh meshData[];
};

void main() {
    Mesh mesh = meshData[fsIn.drawID];
    MaterialData materialData = materials[mesh.materialInstanceIndex];
    Material material = loadMaterial(materialData);
    FragColour = evalMaterial(material);
}