
struct GlobalData {
    float gamma;
    uint time;
    int transformsBufferIndex;
    int meshBufferIndex;
    int materialBufferIndex;
    int lightBufferIndex;
    int cameraBufferIndex;
};

layout (set = 1, binding = 0) uniform Global {
    GlobalData globalData;
};
