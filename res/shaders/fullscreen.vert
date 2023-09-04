
layout (location = 0) out VsOut {
    vec2 TexCoords;
} vsOut;

void main() {
    vsOut.TexCoords = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(vsOut.TexCoords * 2.0f + -1.0f, 0.0f, 1.0);
}