#version 410 core
out vec4 FragColor;
uniform float uSize;

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

float G_SchlickGGX(float NdV, float rough){
    float r = rough + 1.0;
    float k = (r*r)/8.0;
    return NdV / (NdV*(1.0-k)+k);
}

vec2 IntegrateBRDF(float NdV, float rough){
    vec3 V;
    V.x = sqrt(1.0 - NdV*NdV);
    V.y = 0.0;
    V.z = NdV;

    float A = 0.0;
    float B = 0.0;

    vec3 N = vec3(0.0, 0.0, 1.0);

    const uint SAMPLES = 512u;
    for (uint i=0u; i<SAMPLES; ++i){
        vec2 Xi = Hammersley(i, SAMPLES);
        vec3 H  = importanceSampleGGX(Xi, rough, N);
        vec3 L  = normalize(2.0 * dot(V,H) * H - V);

        float NdL = max(L.z, 0.0);
        float NdH = max(H.z, 0.0);
        float VdH = max(dot(V,H), 0.0);

        if (NdL > 0.0) {
            float G = G_SchlickGGX(NdL, rough) * G_SchlickGGX(NdV, rough);
            float G_Vis = (G * VdH) / max(NdH * NdV, 1e-5);
            float Fc = pow(1.0 - VdH, 5.0);
            A += (1.0 - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }
    return vec2(A, B) / float(SAMPLES);
}

void main(){
    vec2 uv = gl_FragCoord.xy / uSize;
    float NdV = uv.x;
    float rough = max(uv.y, 0.04); // avoid zero-roughness singularity
    vec2 integrated = IntegrateBRDF(NdV, rough);
    FragColor = vec4(integrated, 0.0, 1.0);
}
