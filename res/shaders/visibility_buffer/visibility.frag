#extension GL_EXT_mesh_shader : enable

layout (location = 0) in VsOut {
    flat uint drawID;
    flat uint meshletID;
} fsIn;

layout (location = 0) out uvec2 VisibilityInfo;

#include "shaderBridge.h"
#include "bindings.glsl"

#include "visibility_buffer/visibility.glsl"

void main() {
    uint visibility = setMeshletID(fsIn.meshletID);
    visibility |= setPrimitiveID(gl_PrimitiveID);
    VisibilityInfo = uvec2(visibility, fsIn.drawID);
}