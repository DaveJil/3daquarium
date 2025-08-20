#version 410 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;

// per-instance
layout(location=8) in vec3 iBasePos;     // base world position (on floor)
layout(location=9) in vec2 iHeightPhase; // x=height, y=phase
layout(location=10) in vec3 iColor;      // leaf color

uniform mat4 uProj, uView;
uniform float uTime;

out vec3 vWorldPos;
out vec3 vNormal;
out vec3 vColor;

void main() {
    float height = iHeightPhase.x;
    float phase  = iHeightPhase.y;

    // aPos.y in [0..height] (we built mesh with 0..H), sway grows with height
    float t = clamp(aPos.y / max(height, 0.0001), 0.0, 1.0);
    float sway = sin(uTime*1.6 + phase + t*3.1) * (0.04 + 0.06*t);

    vec3 p = aPos;
    p.y = t * height;
    p.x += sway;

    vec3 world = iBasePos + p;
    vWorldPos = world;
    vNormal   = normalize(aNormal); // simple
    vColor    = iColor;
    gl_Position = uProj * uView * vec4(world, 1.0);
}
