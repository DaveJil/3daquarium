#version 410 core
in float vFade;
out vec4 FragColor;

void main() {
    // round bubble using point sprite coords
    vec2 uv = gl_PointCoord * 2.0 - 1.0;
    float r = dot(uv, uv);
    if (r > 1.0) discard;

    // soft edge, small highlight
    float edge = smoothstep(1.0, 0.7, r);
    float highlight = smoothstep(0.15, 0.0, length(uv - vec2(0.4, -0.4)));
    vec3 col = mix(vec3(0.8), vec3(1.0), highlight) * vFade;
    FragColor = vec4(col, edge * 0.7);
}
