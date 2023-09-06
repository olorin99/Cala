
struct Mesh {
    uint firstIndex;
    uint indexCount;
    uint materialIndex;
    uint materialInstanceIndex;
    vec4 min;
    vec4 max;
};

layout (set = 0, binding = 1) readonly buffer MeshBuffer {
    Mesh meshData[];
} bindlessBufferMesh[];
