void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 p = fragCoord.xy / iResolution.xy;
    vec3 color = vec3(0.0);

    if (p.x < 0.5 && p.y < 0.5) {
        color = vec3(0.0, 0.0, 0.0); // bottom-left: black
    } else if (p.x >= 0.5 && p.y < 0.5) {
        color = vec3(0.0, 0.0, 1.0); // bottom-right: blue
    } else if (p.x < 0.5 && p.y >= 0.5) {
        color = vec3(1.0, 0.0, 0.0); // top-left: red
    } else {
        color = vec3(0.0, 1.0, 0.0); // top-right: green
    }

    fragColor = vec4(color, 1.0);
}
