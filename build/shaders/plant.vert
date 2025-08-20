#version 410 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;

// instance data
layout(location=8)  in vec3 iPos;           // base position (at floor)
layout(location=9)  in vec2 iHeightPhase;   // height, sway phase
layout(location=10) in vec3 iColor;         // base color

uniform mat4 uProj, uView;
uniform float uTime;

out vec3 vWorldPos;
out vec3 vNormal;
out vec3 vColor;

void main(){
    float H = iHeightPhase.x;
    float phase = iHeightPhase.y;

    // bend by height along XZ using sine
    float t = clamp(aPos.y / max(H, 0.001), 0.0, 1.0);
    float sway = sin(uTime*0.9 + phase + aPos.y*2.2) * (0.15 + 0.2*t);
    vec3 p = aPos;
    p.x += sway * (0.35 + 0.3*t);

    // scale to height and move to base position
    p.y = p.y; // already in [0..H] in the mesh build
    vec3 world = iPos + vec3(p.x, p.y, p.z);

    vWorldPos = world;
    vNormal   = normalize(vec3(0.0, 0.0, 1.0)); // faux; strip is facing camera-ish
    vColor    = iColor;

    gl_Position = uProj * uView * vec4(world,1.0);
}
