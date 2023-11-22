
layout (location = 0) out VsOut {
    vec3 FragPos;
    vec2 TexCoords;
    mat3 TBN;
    vec3 ViewPos;
    flat uint drawID;
} vsOut;

#include "global_data.glsl"
#include "camera.glsl"


layout (set = 2, binding = 0) uniform FrustumVertices {
    vec4 vertex[8];
};

const uint cubeIndices[] = {
    0, 1, 2, //left
    2, 1, 3,
    0, 5, 1, //top
    0, 4, 5,
    5, 7, 5, //right
    4, 6, 7,
    2, 7, 3, //bottom
    2, 6, 7,
    0, 2, 4, //near
    2, 6, 4,
    7, 5, 1, //far
    1, 3, 7
};

const vec3 frontTopLeft = vec3(-1, 1, 0);
const vec3 frontTopRight = vec3(1, 1, 0);
const vec3 frontBottomLeft = vec3(-1, -1, 0);
const vec3 frontBottomRight = vec3(1, -1, 0);
const vec3 backTopLeft = vec3(-1, 1, 1);
const vec3 backTopRight = vec3(1, 1, 1);
const vec3 backBottomLeft = vec3(-1, -1, 1);
const vec3 backBottomRight = vec3(1, -1, 1);

const vec3 cubeNDC[] = {
//top
    frontTopLeft,
    backTopLeft,
    backTopRight,

    backTopRight,
    frontTopRight,
    frontTopLeft,
//front
    frontBottomLeft,
    frontTopLeft,
    frontTopRight,

    frontTopRight,
    frontBottomRight,
    frontBottomLeft,
//left
    backBottomLeft,
    backTopLeft,
    frontTopLeft,

    frontTopLeft,
    frontBottomLeft,
    backBottomLeft,
//right
    frontBottomRight,
    frontTopRight,
    backTopRight,

    backTopRight,
    backBottomRight,
    frontBottomRight,
//bottom
    backBottomLeft,
    frontBottomLeft,
    frontBottomRight,

    frontBottomRight,
    backBottomRight,
    backBottomLeft,
//back
    backBottomRight,
    backTopRight,
    backTopLeft,

    backTopLeft,
    backBottomLeft,
    backBottomRight,
};

void main() {
    vsOut.FragPos = vec3(0.0);
    vsOut.TexCoords = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    vsOut.TBN = mat3(1.0);
    vsOut.ViewPos = vec3(0.0);
    vsOut.drawID = 0;

    uint index = cubeIndices[gl_VertexIndex];
    vec4 vertex = vertex[index];

    CameraData camera = globalData.cameraBuffer.camera;
    gl_Position = camera.projection * camera.view * vertex;
//    gl_Position = invViewProj * vec4(cubeNDC[gl_VertexIndex], 1.0);
}