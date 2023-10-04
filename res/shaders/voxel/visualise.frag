
layout (location = 0) in VsOut {
    vec2 TexCoords;
} fsIn;

layout (location = 0) out vec4 FragColour;

layout (rgba32f, set = 0, binding = 3) uniform readonly image3D voxelGrid[];

#include "camera.glsl"

#include "global_data.glsl"

layout (push_constant) uniform PushData {
    mat4 voxelOrthographic;
    int voxelGridIndex;
};

vec3 scaleAndBias(vec3 p) {
    return vec3(p.xy * 2 - 1, p.z);
}

bool isInsideCube(vec3 p, float e) {
    return abs(p.x) < 1 + e && abs(p.y) < 1 + e && abs(p.z) < 1 + e;
}

void main() {
    //    if (!isInsideCube(fsIn.FragPos, 0)) {
    //        return;
    //    }

    CameraData camera = bindlessBuffersCamera[globalData.cameraBufferIndex].camera;

    vec3 origin = camera.position;

    vec4 p = inverse(camera.projection * camera.view) * vec4(fsIn.TexCoords * 2 - 1, 0, 1);
    p /= p.w;


    vec3 direction = normalize(p.xyz - origin);

    vec4 colour = vec4(0.0);
//    vec3 colour = direction;

//    for (int step = 1; step < 100 && colour.a < 0.99f; step++) {
////        vec3 currentPos = origin + direction * step * 0.1;
//        vec3 currentPos = origin + direction * step;
////        vec3 currentPos = origin + direction;
//        vec4 i = voxelOrthographic * vec4(currentPos, 1.0);
//        vec3 voxelPos = i.xyz * 0.5 + 0.5;
////        colour += vecvoxelPos;
////        colour = currentPos;
////        if (!isInsideCube(voxelPos, 0)) {
////            colour = vec4(1.0);
////            break;
////        }
//        ivec3 dim = imageSize(voxelGrid[voxelGridIndex]);
//        ivec3 voxelCoords = ivec3(dim * voxelPos);
//        voxelCoords = min(voxelCoords, dim);
//        voxelCoords = max(voxelCoords, ivec3(0));
////        vec4 c = vec4(voxelPos, 1.0);
////        colour = voxelCoords;
//        vec4 c = imageLoad(voxelGrid[voxelGridIndex], voxelCoords);
//        colour += (1.0f - colour.a) * c;
////        colour = c.xyz;
////        if (c != vec4(0.0)) {
////            colour = c.xyz;
////            break;
////        }
//    }

    FragColour = colour;
}