#version 410 core
out vec4 FragColor;

uniform samplerCube uEnv;
uniform float uFaceSize;
uniform int   uFace;
uniform float uRoughness;

// map face uv -> dir
vec3 dirFromFaceUV(int face, vec2 uv){
    if (face==0) return normalize(vec3( 1.0,  uv.y, -uv.x));
    if (face==1) return normalize(vec3(-1.0,  uv.y,  uv.x));
    if (face==2) return normalize(vec3( uv.x,  1.0, -uv.y));
    if (face==3) return normalize(vec3( uv.x, -1.0,  uv.y));
    if (face==4) return normalize(vec3( uv.x,  uv.y,  1.0));
    return           normalize(vec3(-uv.x,  uv.y, -1.0));
}

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
// GGX importance sample
vec3 importanceSampleGGX(vec2 Xi, float rough, vec3 N){
    float a = rough*rough;
    float phi = 2.0*3.14159265*Xi.x;
    float cosT = sqrt((1.0 - Xi.y) / (1.0 + (a*a - 1.0) * Xi.y));
    float sinT = sqrt(1.0 - cosT*cosT);
    vec3 H = vec3(cos(phi)*sinT, sin(phi)*sinT, cosT);
    vec3 up = abs(N.y) < 0.999 ? vec3(0,1,0) : vec3(1,0,0);
    vec3 T = normalize(cross(up, N));
    vec3 B = cross(N, T);
    return normalize(T*H.x + B*H.y + N*H.z);
}

void main(){
    vec2 uv = (gl_FragCoord.xy / uFaceSize) * 2.0 - 1.0;
    vec3 N  = dirFromFaceUV(uFace, uv);
    vec3 V  = N;

    const uint SAMPLES = 256u;
    vec3 prefiltered = vec3(0.0);
    float totalWeight = 0.0;

    for (uint i=0u; i<SAMPLES; ++i){
        vec2 Xi = Hammersley(i, SAMPLES);
        vec3 H  = importanceSampleGGX(Xi, uRoughness, N);
        vec3 L  = normalize(2.0 * dot(V,H) * H - V);
        float NdL = max(dot(N,L), 0.0);
        if (NdL > 0.0) {
            prefiltered += texture(uEnv, L).rgb * NdL;
            totalWeight += NdL;
        }
    }
    prefiltered = prefiltered / max(totalWeight, 1e-4);

    FragColor = vec4(prefiltered, 1.0);
}
