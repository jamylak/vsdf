void mainImage( out vec4 fragColor, in vec2 fragCoord ) {
    // Setup
    vec2 p = fragCoord.xy / iResolution.xy;

    float th = iTime * .5;
    float s = sin(th), c = cos(th);

    vec2 ofs = vec2(
       .5 + p.x + sin(iTime * .01),
       .5 * p.y
    );

    p -= ofs;
    p = vec2(
        c * p.x - s * p.y,
        c * p.y + s * p.x
    );
    p += ofs;

    // Draw
    float f = cos(iTime) * .03 * (.5 - p.y);
    vec2 a = vec2(
        floor(p.x * 10.) / 10. + .05 , 
        floor((p.y + f) * 10.) / 10. + .05 - f
    );
    float d = step(0.005, distance(p, a) - 0.001);
    vec3 col = vec3(d, p);
    fragColor = vec4(col, 1.0);
}
