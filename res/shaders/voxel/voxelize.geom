
layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

layout (location = 0) in VsOut {
    vec3 FragPos;
    vec2 TexCoords;
    mat3 TBN;
    vec3 ViewPos;
    vec3 ClipPos;
    flat uint drawID;
} gsIn[];

layout (location = 0) out GsOut {
    vec3 FragPos;
    vec2 TexCoords;
    mat3 TBN;
    vec3 ViewPos;
    vec3 ClipPos;
    flat uint drawID;
} gsOut;

layout (push_constant) uniform PushData {
    mat4 orthographic;
    uvec4 voxelGridSize;
    uvec4 tileSizes;
    int lightGridIndex;
    int lightIndicesIndex;
    int voxelGridIndex;
};

#define SQRT_2 1.4142135637309

void conservativeTriangle(inout vec3 v0, inout vec3 v1, inout vec3 v2) {
//    vec4 aabb;
//    aabb.x = min(v0.x, min(v1.x, v2.x));
//    aabb.y = min(v0.y, min(v1.y, v2.y));
//    aabb.z = max(v0.z, max(v1.z, v2.z));
//    aabb.w = max(v0.y, max(v1.y, v2.y));

    float resolution = voxelGridSize.x;
//    vec2 hPixel = vec2(1.f / resolution, 1.f / resolution);

    float pl = SQRT_2 / resolution;

//    aabb.xy -= hPixel.xy;
//    aabb.zw += hPixel.xy;

    vec2 e0 = normalize(vec2(v1.xy - v0.xy));
    vec2 e1 = normalize(vec2(v2.xy - v1.xy));
    vec2 e2 = normalize(vec2(v0.xy - v2.xy));

    vec2 n0 = vec2(-e0.y, e0.x);
    vec2 n1 = vec2(-e1.y, e1.x);
    vec2 n2 = vec2(-e2.y, e2.x);

    v0.xy += pl * ((e2 / dot(e2, n0)) + (e0 / dot(e0, n2)));
    v1.xy += pl * ((e0 / dot(e0, n1)) + (e1 / dot(e1, n0)));
    v2.xy += pl * ((e1 / dot(e1, n2)) + (e2 / dot(e2, n1)));
}

void main() {
    vec3 p1 = gsIn[1].ClipPos - gsIn[0].ClipPos;
    vec3 p2 = gsIn[2].ClipPos - gsIn[0].ClipPos;
    vec3 p = abs(cross(p1, p2));

    vec3 v0 = gl_in[0].gl_Position.xyz;
    vec3 v1 = gl_in[1].gl_Position.xyz;
    vec3 v2 = gl_in[2].gl_Position.xyz;

    conservativeTriangle(v0, v1, v2);

    {
        gsOut.FragPos = gsIn[0].FragPos;
        gsOut.TexCoords = gsIn[0].TexCoords;
        gsOut.TBN = gsIn[0].TBN;
        gsOut.ViewPos = gsIn[0].ViewPos;
        gsOut.ClipPos = gsIn[0].ClipPos;
        gsOut.drawID = gsIn[0].drawID;
        if (p.z > p.x && p.z > p.y) {
            gl_Position = vec4(v0.xy, 0, 1);
        } else if (p.x > p.y && p.x > p.z) {
            gl_Position = vec4(v0.yz, 0, 1);
        } else {
            gl_Position = vec4(v0.xz, 0, 1);
        }
        EmitVertex();
    }
    {
        gsOut.FragPos = gsIn[1].FragPos;
        gsOut.TexCoords = gsIn[1].TexCoords;
        gsOut.TBN = gsIn[1].TBN;
        gsOut.ViewPos = gsIn[1].ViewPos;
        gsOut.ClipPos = gsIn[1].ClipPos;
        gsOut.drawID = gsIn[1].drawID;
        if (p.z > p.x && p.z > p.y) {
            gl_Position = vec4(v1.xy, 0, 1);
        } else if (p.x > p.y && p.x > p.z) {
            gl_Position = vec4(v1.yz, 0, 1);
        } else {
            gl_Position = vec4(v1.xz, 0, 1);
        }
        EmitVertex();
    }
    {
        gsOut.FragPos = gsIn[2].FragPos;
        gsOut.TexCoords = gsIn[2].TexCoords;
        gsOut.TBN = gsIn[2].TBN;
        gsOut.ViewPos = gsIn[2].ViewPos;
        gsOut.ClipPos = gsIn[2].ClipPos;
        gsOut.drawID = gsIn[2].drawID;
        if (p.z > p.x && p.z > p.y) {
            gl_Position = vec4(v2.xy, 0, 1);
        } else if (p.x > p.y && p.x > p.z) {
            gl_Position = vec4(v2.yz, 0, 1);
        } else {
            gl_Position = vec4(v2.xz, 0, 1);
        }
        EmitVertex();
    }
    EndPrimitive();
}