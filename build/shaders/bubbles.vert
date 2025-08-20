#version 410 core
layout(location=0) in vec3 aPos;   // per-bubble world position

uniform mat4 uProj, uView;

out float vFade;

void main() {
    vec4 viewPos = uView * vec4(aPos, 1.0);
    float dist = length(viewPos.xyz);
    gl_Position = uProj * viewPos;

    // size attenuates slightly with distance
    gl_PointSize = clamp(8.0 / (0.3 + 0.1 * dist), 2.0, 10.0);
    vFade = clamp(1.0 / (0.4 + 0.15 * dist), 0.2, 1.0);
}
