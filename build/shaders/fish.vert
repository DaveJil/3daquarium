#version 410 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;

layout(location=3) in vec3 iPos;
layout(location=4) in vec3 iDir;
layout(location=5) in vec2 iPhaseScale;
layout(location=6) in vec3 iStretch;
layout(location=7) in vec3 iColor;
layout(location=8) in float iSpecies;

uniform mat4 uProj, uView;
uniform float uTime;

out vec3 vWorldPos;
out vec3 vNormal;
out vec3 vColor;
out float vLen;
out vec3 vLocal;
out float vSpecies;

out vec3 vTanU;
out vec3 vTanV;

out float vFinMask;
out vec2  vFinUV;

vec3 safe_normalize(vec3 v){ float l=length(v); return (l>1e-6)? v/l : vec3(0,0,-1); }

void main(){
    float phase = iPhaseScale.x;
    float scale = iPhaseScale.y;

    float len = clamp(aPos.x, 0.0, 1.0);
    float sway = sin(uTime * 5.0 + phase + len*1.6) * 0.28 * len;
    vec3 p = aPos; p.z += sway * len * 0.4;
    vec3 scaled = p * (iStretch * scale);

    vec3 f = safe_normalize(iDir);
    vec3 up = abs(dot(f, vec3(0,1,0))) > 0.95 ? vec3(1,0,0) : vec3(0,1,0);
    vec3 r = normalize(cross(up, f));
    vec3 u = cross(f, r);
    mat3 B = mat3(r, u, f);

    vec3 world = B * scaled + iPos;

    vWorldPos = world;
    vNormal   = normalize(B * aNormal);
    vColor    = iColor;
    vLen      = len;
    vLocal    = scaled;
    vSpecies  = iSpecies;

    vTanU = normalize(f);
    vTanV = normalize(r);

    bool isFin = (abs(aNormal.z) > 0.95 && aPos.x > 0.98);
    vFinMask = isFin ? 1.0 : 0.0;
    vFinUV = vec2(scaled.y, scaled.z) * 6.0;

    gl_Position = uProj * uView * vec4(world, 1.0);
}
