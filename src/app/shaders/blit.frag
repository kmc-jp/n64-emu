#version 450
// Blit a single sampled texture to the swapchain.

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 oColor;

layout(set = 0, binding = 0) uniform sampler2D uTex;

void main() {
    oColor = texture(uTex, vUV);
}
