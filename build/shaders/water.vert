#version 410 core
layout(location=0) in vec3 aPos;
layout(location=2) in vec2 aUV;

uniform mat4 uProj, uView, uModel;
uniform float uTime;

out vec3 vWorldPos;
out float vFoam;

void main() {
    // two simple wave layers
    float f1 = 8.0, a1 = 0.02, s1 = 1.0;
    float f2 = 3.5, a2 = 0.015, s2 = 0.6;

    float wave = a1 * sin(aUV.x * f1 + uTime * 1.4 * s1)
               + a2 * sin(aUV.y * f2 - uTime * 0.9 * s2);

    vec3 pos = aPos + vec3(0.0, wave, 0.0);
    vFoam = smoothstep(0.02, 0.045, abs(wave));

    vec4 world = uModel * vec4(pos, 1.0);
    vWorldPos = world.xyz;
    gl_Position = uProj * uView * world;
}
