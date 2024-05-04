#version 450

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

void main() {
    color = vec4(1.0, 0.0, 0.0, 1.0);;
}

