#version 410 core
out vec4 FragColor;

uniform samplerCube uEnv;
uniform float uFaceSize;
uniform int   uFace;

vec3 dirFromFaceUV(int face, vec2 uv){
    if (face==0) return normalize(vec3( 1.0,  uv.y, -uv.x));
    if (face==1) return normalize(vec3(-1.0,  uv.y,  uv.x));
    if (face==2) return normalize(vec3( uv.x,  1.0, -uv.y));
    if (face==3) return normalize(vec3( uv.x, -1.0,  uv.y));
    if (face==4) return normalize(vec3( uv.x,  uv.y,  1.0));
    return           normalize(vec3(-uv.x,  uv.y, -1.0));
}

// Hemisphere sampling (Lambert)
float RadicalInverse_VdC(uint bits){
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}
vec2 Hammersley(uint i, uint N){
    return vec2(float(i)/float(N), RadicalInverse_VdC(i));
}
// cosine-weighted hemisphere oriented to N
vec3 sampleHemisphere(vec3 N, vec2 Xi){
    float phi = 2.0*3.14159265*Xi.x;
    float cosT = sqrt(1.0 - Xi.y);
    float sinT = sqrt(1.0 - cosT*cosT);
    vec3 H = vec3(cos(phi)*sinT, sin(phi)*sinT, cosT);
    // build basis
    vec3 up = abs(N.y) < 0.999 ? vec3(0,1,0) : vec3(1,0,0);
    vec3 T = normalize(cross(up, N));
    vec3 B = cross(N, T);
    return normalize(T*H.x + B*H.y + N*H.z);
}

void main(){
    vec2 uv = (gl_FragCoord.xy / uFaceSize) * 2.0 - 1.0;
    vec3 N  = dirFromFaceUV(uFace, uv);

    const uint SAMPLES = 128u;
    vec3 col = vec3(0.0);
    for (uint i=0u; i<SAMPLES; ++i){
        vec2 Xi = Hammersley(i, SAMPLES);
        vec3 L  = sampleHemisphere(N, Xi);
        float NdL = max(dot(N,L),0.0);
        col += texture(uEnv, L).rgb * NdL;
    }
    col = col * (1.0 / float(SAMPLES)) * (1.0/3.14159265);

    FragColor = vec4(col, 1.0);
}
