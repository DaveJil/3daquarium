#version 410 core
out vec4 FragColor;

uniform float uFaceSize;   // texture size (float to avoid int ops in GLSL 410)
uniform int   uFace;       // 0..5

// Map face + fragcoord -> direction
vec3 dirFromFaceUV(int face, vec2 uv){
    // uv in [-1,1]
    if (face==0) return normalize(vec3( 1.0,  uv.y, -uv.x)); // +X
    if (face==1) return normalize(vec3(-1.0,  uv.y,  uv.x)); // -X
    if (face==2) return normalize(vec3( uv.x,  1.0, -uv.y)); // +Y
    if (face==3) return normalize(vec3( uv.x, -1.0,  uv.y)); // -Y
    if (face==4) return normalize(vec3( uv.x,  uv.y,  1.0)); // +Z
    return           normalize(vec3(-uv.x,  uv.y, -1.0));    // -Z
}

// Simple procedural underwater HDR: blue gradient + bright “caustic sun” cone
vec3 envColor(vec3 d){
    float up = clamp(d.y*0.5 + 0.5, 0.0, 1.0);
    vec3 top    = vec3(0.35,0.75,1.0);
    vec3 middle = vec3(0.10,0.25,0.45);
    vec3 bottom = vec3(0.02,0.05,0.08);
    vec3 sky    = mix(bottom, mix(middle, top, smoothstep(0.2,0.9,up)), smoothstep(0.0,1.0,up));

    // “sun” shaft
    vec3 sunDir = normalize(vec3(-0.2, 0.9, 0.1));
    float sun   = pow(max(dot(d, sunDir), 0.0), 900.0) * 8.0; // HDR
    return sky + vec3(sun);
}

void main(){
    vec2 uv = (gl_FragCoord.xy / uFaceSize) * 2.0 - 1.0;   // [0,1]->[-1,1]
    vec3 dir = dirFromFaceUV(uFace, uv);
    FragColor = vec4(envColor(dir), 1.0);
}
