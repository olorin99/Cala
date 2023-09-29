
layout (location = 0) in VsOut {
    vec2 TexCoords;
} fsIn;

layout (location = 0) out vec4 FragColour;

layout (set = 0, binding = 0) uniform textureCube cubeMaps[];
layout (set = 0, binding = 0) uniform texture2D textureMaps[];
layout (set = 0, binding = 2) uniform sampler samplers[];
layout (rgba32f, set = 0, binding = 3) uniform readonly image3D voxelGrid[];

#include "camera.glsl"

#include "global_data.glsl"

layout (push_constant) uniform PushData {
    int voxelGridIndex;
};

vec3 scaleAndBias(vec3 p) {
    return vec3(p.xy * 0.5 + 0.5, p.z);
}

bool isInsideCube(vec3 p, float e) {
    return abs(p.x) < 1 + e && abs(p.y) < 1 + e && abs(p.z) < 1 + e;
}

void main() {
    //    if (!isInsideCube(fsIn.FragPos, 0)) {
    //        return;
    //    }

//    CameraData camera = bindlessBuffersCamera[globalData.cameraBufferIndex].camera;

//    vec3 origin = camera.position;

//    vec4 p = camera.view * vec4(fsIn.TexCoords, 0, 1);

//    vec3 direction = p.xyz - origin;
//    vec3 direction = p.xyz;

//    vec4 unprojected = camera.projection * camera.view * vec4(fsIn.TexCoords, 0, 1);
//    unprojected.xyz /= unprojected.w;
//    vec3 direction = unprojected.xyz - origin;
    vec3 direction = vec3(fsIn.TexCoords, 1);
//    vec3 direction = unprojected.xyz;

//    vec3 voxelPos = scaleAndBias(fsIn.FragPos);
//    ivec3 dim = imageSize(voxelGrid[voxelGridIndex]);
//    ivec3 voxelCoords = ivec3(dim * voxelPos);
//    voxelCoords = min(voxelCoords, dim);
//    imageStore(voxelGrid[voxelGridIndex], voxelCoords, colour);
    //    FragColour = ivec4(dim * voxel, 1.0);
    //    FragColour = colour;
    //    FragColour = vec4(fsIn.FragPos, 1.0);
    FragColour = vec4(direction, 1.0);
    //    FragColour = vec4(gl_FragCoord.xyz, 1.0);
    //    FragColour = vec4(gl_FragCoord.xy, gl_FragCoord.z * dim.z, 1.0);
    //    FragColour = vec4(dim, 1.0);
    //    FragColour = vec4(dim * voxel, 1.0);
}