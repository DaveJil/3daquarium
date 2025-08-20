#version 410 core
layout(location=0) in vec3 aPos;
layout(location=2) in vec2 aUV;

uniform mat4 uProj, uView, uModel;
uniform float uTime;

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vScreenUV;

float waveFn(vec2 p, float t) {
    // two gentle wave fields (world units)
    float w = 0.0;
    w += 0.010 * sin(dot(p, vec2( 1.8,  1.1)) * 6.0 + t * 1.4);
    w += 0.006 * sin(dot(p, vec2(-1.2,  2.3)) * 3.8 - t * 0.9);
    return w;
}

// finite differences to estimate normal
vec3 surfaceNormal(vec3 pos, float t) {
    float eps = 0.02;
    float h  = waveFn(pos.xz, t);
    float hx = waveFn(pos.xz + vec2(eps,0.0), t);
    float hz = waveFn(pos.xz + vec2(0.0,eps), t);
    float dhdx = (hx - h)/eps;
    float dhdz = (hz - h)/eps;
    vec3 n = normalize(vec3(-dhdx, 1.0, -dhdz));
    return n;
}

void main() {
    float t = uTime;
    vec3 p = aPos;
    p.y += waveFn(p.xy, t);           // lift by wave

    vec4 world = uModel * vec4(p, 1.0);
    vWorldPos = world.xyz;
    vNormal   = surfaceNormal(p, t);  // (model is identity in this scene)

    vec4 clip = uProj * uView * world;
    vScreenUV = clip.xy/clip.w * 0.5 + 0.5;

    gl_Position = clip;
}
