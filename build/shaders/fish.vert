#version 410 core
layout(location=0) in vec3 aPos;      // local model position
layout(location=1) in vec3 aNormal;   // local normal

// per-instance data
layout(location=3) in vec3 iPos;          // world pos
layout(location=4) in vec3 iDir;          // facing dir
layout(location=5) in vec2 iPhaseScale;   // x: phase, y: base scale
layout(location=6) in vec3 iStretch;      // non-uniform scale (x,y,z)
layout(location=7) in vec3 iColor;        // base color

uniform mat4 uProj, uView;
uniform float uTime;

out vec3 vWorldPos;
out vec3 vNormal;
out vec3 vColor;

vec3 safe_normalize(vec3 v) {
    float l = length(v); return (l>1e-6) ? v/l : vec3(0,0,-1);
}

void main() {
    float len = clamp(aPos.x, 0.0, 1.0);
    float sway = sin(uTime * 5.0 + iPhaseScale.x) * 0.25 * len;
    vec3 p = aPos;
    p.z += sway * len * 0.35;

    vec3 scaled = p * (iStretch * iPhaseScale.y);

    vec3 f = safe_normalize(iDir);
    vec3 up = vec3(0.0, 1.0, 0.0);
    if (abs(dot(f, up)) > 0.95) up = vec3(1.0, 0.0, 0.0);
    vec3 r = normalize(cross(up, f));
    vec3 u = cross(f, r);
    mat3 B = mat3(r, u, f);

    vec3 world = B * scaled + iPos;
    vWorldPos = world;
    vNormal = normalize(B * aNormal);
    vColor  = iColor;
    gl_Position = uProj * uView * vec4(world, 1.0);
}
