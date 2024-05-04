#version 450

// ALl setup needed to make most things work
// eg. for a shader toy shader.
// Not everything yet...

layout (push_constant) uniform PushConstants {
    float iTime;
    int iFrame;
    vec2 iResolution;
    vec2 iMouse;
} pc;

layout (location = 0) in vec2 TexCoord;
layout (location = 0) out vec4 color;

#define iTime pc.iTime
#define iResolution pc.iResolution
#define iFrame pc.iFrame
#define iMouse pc.iMouse

void mainImage(out vec4 fragColor, in vec2 fragCoord);
void main() {
    // Call your existing mainImage function
    vec4 fragColor;
    // Convert from vulkan to glsl
    mainImage(fragColor, vec2(gl_FragCoord.x, iResolution.y - gl_FragCoord.y));
    // Output color
    color = fragColor;
}

