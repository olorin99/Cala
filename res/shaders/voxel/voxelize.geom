
layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

layout (location = 0) in VsOut {
    vec3 FragPos;
    vec2 TexCoords;
    mat3 TBN;
    vec3 ViewPos;
    flat uint drawID;
} gsIn[];

layout (location = 0) out GsOut {
    vec3 FragPos;
    vec2 TexCoords;
    mat3 TBN;
    vec3 ViewPos;
    flat uint drawID;
} gsOut;

void main() {
    vec3 p1 = gsIn[1].FragPos - gsIn[0].FragPos;
    vec3 p2 = gsIn[2].FragPos - gsIn[0].FragPos;
    vec3 p = abs(cross(p1, p2));
    for (int i = 0; i < gl_in.length(); i++) {
        gsOut.FragPos = gsIn[i].FragPos;
        gsOut.TexCoords = gsIn[i].TexCoords;
        gsOut.TBN = gsIn[i].TBN;
        gsOut.ViewPos = gsIn[i].ViewPos;
        gsOut.drawID = gsIn[i].drawID;
        if (p.z > p.x && p.z > p.y) {
            gl_Position = vec4(gl_in[i].gl_Position.xy, 0, 1);
        } else if (p.x > p.y && p.x > p.z) {
            gl_Position = vec4(gl_in[i].gl_Position.yz, 0, 1);
        } else {
            gl_Position = vec4(gl_in[i].gl_Position.xz, 0, 1);
        }
        EmitVertex();
    }
    EndPrimitive();
}