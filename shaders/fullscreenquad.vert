#version 450

layout(location = 0) out vec2 texCoord;

const vec2 vertices[6] = vec2[](
    vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(1.0, 1.0),
    vec2(-1.0, -1.0), vec2(1.0, 1.0), vec2(-1.0, 1.0)
);

void main() {
    uint index = gl_VertexIndex % 6;  // Ensure the index wraps around if needed
    gl_Position = vec4(vertices[index], 0.0, 1.0);
    texCoord = vertices[index] * 0.5 + 0.5;
}
