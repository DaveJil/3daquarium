#version 410 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;

// per-instance
layout(location=3) in vec3 iPos;
layout(location=4) in vec3 iDir;
layout(location=5) in vec2 iPhaseScale;  // phase, scale
layout(location=6) in vec3 iStretch;     // anisotropic scale
layout(location=7) in vec3 iColor;       // base tint
layout(location=8) in float iSpecies;    // 0 clown, 1 neon, 2 danio

uniform mat4 uProj, uView;
uniform float uTime;

out vec3 vWorldPos;
out vec3 vNormal;
out vec3 vColor;
out float vLen;       // 0..1 along body (x)
out vec3 vLocal;      // local (animated, stretched)
out float vSpecies;

// Tangent frame for bump/normal mapping on body/fins
out vec3 vTanU;       // along body (forward)
out vec3 vTanV;       // around body (circumference)

// Fin data
out float vFinMask;   // 1 for fin triangles, else 0
out vec2  vFinUV;     // local UV for fin patterns

vec3 safe_normalize(vec3 v){ float l=length(v); return (l>1e-6)? v/l : vec3(0,0,-1); }

void main(){
    float phase = iPhaseScale.x;
    float scale = iPhaseScale.y;

    float len = clamp(aPos.x, 0.0, 1.0);

    // swim (tail sway)
    float sway = sin(uTime * 5.0 + phase + len*1.6) * 0.28 * len;
    vec3 p = aPos;
    p.z += sway * len * 0.4;

    // anisotropic scaling of the base body
    vec3 scaled = p * (iStretch * scale);

    // basis from swimming direction
    vec3 f = safe_normalize(iDir);                 // forward
    vec3 up = abs(dot(f, vec3(0,1,0))) > 0.95 ? vec3(1,0,0) : vec3(0,1,0);
    vec3 r = normalize(cross(up, f));              // “right” (around body)
    vec3 u = cross(f, r);                          // “up”  (completes basis)
    mat3 B = mat3(r, u, f);                        // columns: r,u,f

    vec3 world = B * scaled + iPos;

    // outputs
    vWorldPos = world;
    vNormal   = normalize(B * aNormal);
    vColor    = iColor;
    vLen      = len;
    vLocal    = scaled;
    vSpecies  = iSpecies;

    // tangent frame (world)
    vTanU = normalize(f);   // along body
    vTanV = normalize(r);   // around body

    // Identify tail-fin triangles: our mesh gave them ±Z normals and x≈1
    bool isFin = (abs(aNormal.z) > 0.95 && aPos.x > 0.98);
    vFinMask = isFin ? 1.0 : 0.0;

    // Fin UVs in local space (scaled for nice ripple frequency)
    vFinUV = vec2(scaled.y, scaled.z) * 6.0;

    gl_Position = uProj * uView * vec4(world, 1.0);
}
