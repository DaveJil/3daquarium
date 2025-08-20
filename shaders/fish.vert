#version 410 core
layout(location=0) in vec3 aPos;      // fish model position (local)
layout(location=1) in vec3 aNormal;   // fish model normal (local)

// per-instance data
layout(location=3) in vec3 iPos;          // world position of fish
layout(location=4) in vec3 iDir;          // facing direction (normalized)
layout(location=5) in vec2 iPhaseScale;   // x = phase, y = scale

uniform mat4 uProj, uView;
uniform float uTime;

out vec3 vWorldPos;
out vec3 vNormal;

vec3 safe_normalize(vec3 v) {
    float l = length(v);
    return l > 1e-6 ? v / l : vec3(0,0,-1);
}

void main() {
    // bend tail left-right using local x as "length along body" (0=head, 1=tail)
    float len = clamp(aPos.x, 0.0, 1.0);
    float sway = sin(uTime * 5.0 + iPhaseScale.x) * 0.25 * len;
    vec3 p = aPos;
    p.z += sway * len * 0.35; // lateral tail motion

    // orient the fish using iDir
    vec3 f = safe_normalize(iDir);                     // forward
    vec3 up = vec3(0.0, 1.0, 0.0);
    // handle near-vertical edge case
    if (abs(dot(f, up)) > 0.95) up = vec3(1.0, 0.0, 0.0);
    vec3 r = normalize(cross(up, f));                  // right
    vec3 u = cross(f, r);                              // true up
    mat3 basis = mat3(r, u, f);

    float s = max(iPhaseScale.y, 0.05);                // scale (size)
    vec3 world = basis * (p * s) + iPos;

    vWorldPos = world;
    vNormal = normalize(basis * aNormal);
    gl_Position = uProj * uView * vec4(world, 1.0);
}
