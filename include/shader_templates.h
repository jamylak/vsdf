#pragma once

namespace shader_templates {

constexpr const char kDefaultTemplate[] =
    R"(void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = fragCoord / iResolution.xy;
    vec3 col = 0.5 + 0.5 * cos(iTime + uv.xyx + vec3(0, 2, 4));
    fragColor = vec4(col, 1.0);
}
)";

constexpr const char kPlotTemplate[] = R"(// 2D function plotter template
// Edit the function below to plot your own math!

float func(float x) {
    return sin(x * 3.0 + iTime) * 0.3;
}

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = (fragCoord - 0.5 * iResolution.xy) / iResolution.y;
    
    // Grid
    vec3 col = vec3(0.1);
    float gridSize = 0.1;
    if (mod(uv.x, gridSize) < 0.002 || mod(uv.y, gridSize) < 0.002) {
        col = vec3(0.2);
    }
    
    // Axes
    if (abs(uv.x) < 0.002) col = vec3(0.0, 0.5, 0.0);
    if (abs(uv.y) < 0.002) col = vec3(0.5, 0.0, 0.0);
    
    // Plot the function
    float x = uv.x * 4.0;
    float y = func(x);
    float dist = abs(uv.y - y);
    
    if (dist < 0.01) {
        col = vec3(1.0, 0.8, 0.2);
    }
    
    fragColor = vec4(col, 1.0);
}
)";

} // namespace shader_templates
