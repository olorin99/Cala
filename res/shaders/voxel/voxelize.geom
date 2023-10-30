
layout(triangles) in;
layout(triangle_strip, max_vertices = 9) out;

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
    uvec4 voxelGridSize;
    uvec4 tileSizes;
    int lightGridIndex;
    int lightIndicesIndex;
};

#define SQRT_2 1.4142135637309

int selectDominantAxis(inout vec3 v0, inout vec3 v1, inout vec3 v2, inout vec2 u0, inout vec2 u1, inout vec2 u2) {
    vec3 n = cross(v1 - v0, v2 - v0);
    vec3 l = abs(n);

    if (l.x >= l.y && l.x >= l.z) {
        v0 = v0.yzx;
        v1 = v1.yzx;
        v2 = v2.yzx;
        if (n.x > 0) {
            vec3 tmp0 = v0;
            v0 = v1;
            v1 = tmp0;

            vec2 tmp1 = u0;
            u0 = u1;
            u1 = tmp1;
        }
        return 0;
    }
    if (l.y >= l.x && l.y >= l.z) {
        v0 = v0.xzy;
        v1 = v1.xzy;
        v2 = v2.xzy;
        if (n.y < 0) {
            vec3 tmp0 = v0;
            v0 = v1;
            v1 = tmp0;

            vec2 tmp1 = u0;
            u0 = u1;
            u1 = tmp1;
        }
        return 1;
    }

    if (n.z > 0) {
        vec3 tmp0 = v0;
        v0 = v1;
        v1 = tmp0;

        vec2 tmp1 = u0;
        u0 = u1;
        u1 = tmp1;
    }
    return 2;
}

void conservativeTriangle(inout vec3 v0, inout vec3 v1, inout vec3 v2) {
    float resolution = voxelGridSize.x;

    float pl = SQRT_2 / resolution;

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
    vec3 v0 = gl_in[0].gl_Position.xyz;
    vec3 v1 = gl_in[1].gl_Position.xyz;
    vec3 v2 = gl_in[2].gl_Position.xyz;

    vec3 p1 = v1 - v0;
    vec3 p2 = v2 - v0;
    vec3 p = abs(normalize(cross(p1, p2)));

//    vec3 N = gsIn[0].TBN[2];
//    vec3 p = abs(N);

    vec2 uv0 = gsIn[0].TexCoords;
    vec2 uv1 = gsIn[1].TexCoords;
    vec2 uv2 = gsIn[2].TexCoords;

//    int swizz = selectDominantAxis(v0, v1, v2, uv0, uv1, uv2);

//    conservativeTriangle(v0, v1, v2);

    {
        gsOut.FragPos = gsIn[0].FragPos;
        gsOut.TexCoords = uv0;
        gsOut.TBN = gsIn[0].TBN;
        gsOut.ViewPos = gsIn[0].ViewPos;
        gsOut.ClipPos = gsIn[0].ClipPos;
        gsOut.drawID = gsIn[0].drawID;
        if (p.x >= p.y && p.x >= p.z) {
            gl_Position = vec4(v0.yzx, 1);
//            gl_Position = vec4(0, 0, 0, 1);
        } else if (p.y >= p.x && p.y >= p.z) {
            gl_Position = vec4(v0.xzy, 1);
//            gl_Position = vec4(0, 0, 0, 1);
        } else {
            gl_Position = vec4(v0.xyz, 1);
        }
//        gl_Position = vec4(p.xyz, 1);

//        if (p.z > p.x && p.z > p.y) {
//            gl_Position = vec4(v0.xy, 0, 1);
//        } else if (p.x > p.y && p.x > p.z) {
//            gl_Position = vec4(v0.yz, 0, 1);
//        } else {
//            gl_Position = vec4(v0.xz, 0, 1);
//        }
        EmitVertex();
    }
    {
        gsOut.FragPos = gsIn[1].FragPos;
        gsOut.TexCoords = uv1;
        gsOut.TBN = gsIn[1].TBN;
        gsOut.ViewPos = gsIn[1].ViewPos;
        gsOut.ClipPos = gsIn[1].ClipPos;
        gsOut.drawID = gsIn[1].drawID;
        if (p.x >= p.y && p.x >= p.z) {
            gl_Position = vec4(v1.yzx, 1);
        } else if (p.y >= p.x && p.y >= p.z) {
            gl_Position = vec4(v1.xzy, 1);
        } else {
            gl_Position = vec4(v1.xyz, 1);
        }
//        gl_Position = vec4(p.xyz, 1);
        //        if (p.z > p.x && p.z > p.y) {
//            gl_Position = vec4(v1.xy, 0, 1);
//        } else if (p.x > p.y && p.x > p.z) {
//            gl_Position = vec4(v1.yz, 0, 1);
//        } else {
//            gl_Position = vec4(v1.xz, 0, 1);
//        }
        EmitVertex();
    }
    {
        gsOut.FragPos = gsIn[2].FragPos;
        gsOut.TexCoords = uv2;
        gsOut.TBN = gsIn[2].TBN;
        gsOut.ViewPos = gsIn[2].ViewPos;
        gsOut.ClipPos = gsIn[2].ClipPos;
        gsOut.drawID = gsIn[2].drawID;
        if (p.x >= p.y && p.x >= p.z) {
            gl_Position = vec4(v2.yzx, 1);
        } else if (p.y >= p.x && p.y >= p.z) {
            gl_Position = vec4(v2.xzy, 1);
        } else {
            gl_Position = vec4(v2.xyz, 1);
        }
//        gl_Position = vec4(p.xyz, 1);
        //        if (p.z > p.x && p.z > p.y) {
//            gl_Position = vec4(v2.xy, 0, 1);
//        } else if (p.x > p.y && p.x > p.z) {
//            gl_Position = vec4(v2.yz, 0, 1);
//        } else {
//            gl_Position = vec4(v2.xz, 0, 1);
//        }
        EmitVertex();
    }
    EndPrimitive();




//    {
//        gsOut.FragPos = gsIn[0].FragPos.yzx;
//        gsOut.TexCoords = uv0;
//        gsOut.TBN = gsIn[0].TBN;
//        gsOut.ViewPos = gsIn[0].ViewPos;
//        gsOut.ClipPos = gsIn[0].ClipPos;
//        gsOut.drawID = gsIn[0].drawID;
//
//        gl_Position = vec4(v0.yzx, 1.0);
//        EmitVertex();
//    }
//    {
//        gsOut.FragPos = gsIn[1].FragPos.yzx;
//        gsOut.TexCoords = uv1;
//        gsOut.TBN = gsIn[1].TBN;
//        gsOut.ViewPos = gsIn[1].ViewPos;
//        gsOut.ClipPos = gsIn[1].ClipPos;
//        gsOut.drawID = gsIn[1].drawID;
//
//        gl_Position = vec4(v1.yzx, 1.0);
//        EmitVertex();
//    }
//    {
//        gsOut.FragPos = gsIn[2].FragPos.yzx;
//        gsOut.TexCoords = uv2;
//        gsOut.TBN = gsIn[2].TBN;
//        gsOut.ViewPos = gsIn[2].ViewPos;
//        gsOut.ClipPos = gsIn[2].ClipPos;
//        gsOut.drawID = gsIn[2].drawID;
//
//        gl_Position = vec4(v2.yzx, 1.0);
//        EmitVertex();
//    }
//    EndPrimitive();
//
//    {
//        gsOut.FragPos = gsIn[0].FragPos.xzy;
//        gsOut.TexCoords = uv0;
//        gsOut.TBN = gsIn[0].TBN;
//        gsOut.ViewPos = gsIn[0].ViewPos;
//        gsOut.ClipPos = gsIn[0].ClipPos;
//        gsOut.drawID = gsIn[0].drawID;
//
//        gl_Position = vec4(v0.xzy, 1.0);
//        EmitVertex();
//    }
//    {
//        gsOut.FragPos = gsIn[1].FragPos.xzy;
//        gsOut.TexCoords = uv1;
//        gsOut.TBN = gsIn[1].TBN;
//        gsOut.ViewPos = gsIn[1].ViewPos;
//        gsOut.ClipPos = gsIn[1].ClipPos;
//        gsOut.drawID = gsIn[1].drawID;
//
//        gl_Position = vec4(v1.xzy, 1.0);
//        EmitVertex();
//    }
//    {
//        gsOut.FragPos = gsIn[2].FragPos.xzy;
//        gsOut.TexCoords = uv2;
//        gsOut.TBN = gsIn[2].TBN;
//        gsOut.ViewPos = gsIn[2].ViewPos;
//        gsOut.ClipPos = gsIn[2].ClipPos;
//        gsOut.drawID = gsIn[2].drawID;
//
//        gl_Position = vec4(v2.xzy, 1.0);
//        EmitVertex();
//    }
//    EndPrimitive();
//
//    {
//        gsOut.FragPos = gsIn[0].FragPos.xyz;
//        gsOut.TexCoords = uv0;
//        gsOut.TBN = gsIn[0].TBN;
//        gsOut.ViewPos = gsIn[0].ViewPos;
//        gsOut.ClipPos = gsIn[0].ClipPos;
//        gsOut.drawID = gsIn[0].drawID;
//
//        gl_Position = vec4(v0.xyz, 1.0);
//        EmitVertex();
//    }
//    {
//        gsOut.FragPos = gsIn[1].FragPos.xyz;
//        gsOut.TexCoords = uv1;
//        gsOut.TBN = gsIn[1].TBN;
//        gsOut.ViewPos = gsIn[1].ViewPos;
//        gsOut.ClipPos = gsIn[1].ClipPos;
//        gsOut.drawID = gsIn[1].drawID;
//
//        gl_Position = vec4(v1.xyz, 1.0);
//        EmitVertex();
//    }
//    {
//        gsOut.FragPos = gsIn[2].FragPos.xyz;
//        gsOut.TexCoords = uv2;
//        gsOut.TBN = gsIn[2].TBN;
//        gsOut.ViewPos = gsIn[2].ViewPos;
//        gsOut.ClipPos = gsIn[2].ClipPos;
//        gsOut.drawID = gsIn[2].drawID;
//
//        gl_Position = vec4(v2.xyz, 1.0);
//        EmitVertex();
//    }
//    EndPrimitive();
}