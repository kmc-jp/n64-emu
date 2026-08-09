#version 450
// Fullscreen oversized triangle (3 vertices). UVs are derived from clip position.

layout(location = 0) out vec2 vUV;

void main() {
    vec4 pos;
    if (gl_VertexIndex == 0)
        pos = vec4(-1.0, -1.0, 0.0, 1.0);
    else if (gl_VertexIndex == 1)
        pos = vec4(-1.0, 3.0, 0.0, 1.0);
    else
        pos = vec4(3.0, -1.0, 0.0, 1.0);

    gl_Position = pos;
    vUV = pos.xy * 0.5 + vec2(0.5);
}
