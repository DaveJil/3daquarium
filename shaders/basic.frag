#version 410 core
in vec3 vWorldPos;
in vec3 vNormal;

uniform vec3 uLightDir;   // direction TOWARDS light
uniform vec3 uViewPos;
uniform vec3 uBaseColor;  // per draw (sand color, rock tint, glass tint)
uniform vec3 uFogColor;
uniform float uFogNear, uFogFar;
uniform float uTime;

uniform int   uApplyCaustics; // 0/1
uniform float uAlpha;         // glass alpha comes from app
uniform int   uMaterialType;  // 0=sand/glass, 1=rock

out vec4 FragColor;

const float PI = 3.14159265359;

float fogFactor(float d){ return clamp((uFogFar - d) / (uFogFar - uFogNear), 0.0, 1.0); }
float sat(float x){ return clamp(x,0.0,1.0); }

// PBR helpers (GGX / Schlick)
vec3 F_Schlick(float cosTheta, vec3 F0){
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}
float D_GGX(vec3 N, vec3 H, float rough){
    float a  = rough*rough;
    float a2 = a*a;
    float NdH = max(dot(N,H), 0.0);
    float NdH2 = NdH*NdH;
    float denom = NdH2 * (a2 - 1.0) + 1.0;
    return a2 / max(PI * denom * denom, 1e-6);
}
float G_SchlickGGX(float NdV, float rough){
    float r = rough + 1.0;
    float k = (r*r) / 8.0;
    return NdV / (NdV * (1.0 - k) + k);
}
float G_Smith(vec3 N, vec3 V, vec3 L, float rough){
    float NdV = max(dot(N,V),0.0);
    float NdL = max(dot(N,L),0.0);
    return G_SchlickGGX(NdV, rough) * G_SchlickGGX(NdL, rough);
}

// Tiny hash noise (for rock micro detail)
float n3(vec3 p){ return fract(sin(dot(p, vec3(12.9898,78.233,37.719))) * 43758.5453); }

// Caustics used for sand
float caustic(vec3 p){
    float t = uTime * 0.8;
    float v = 0.0;
    v += sin(dot(p.xz, vec2( 4.2, 3.6)) + t*1.8);
    v += sin(dot(p.xz, vec2(-3.4, 2.7)) - t*1.2);
    v = v*0.5 + 0.5;
    return smoothstep(0.75, 0.95, v);
}

// Ambient “IBL-ish” gradient
vec3 evalAmbient(vec3 n){
    vec3 top    = vec3(0.10, 0.22, 0.34);
    vec3 middle = vec3(0.04, 0.10, 0.16);
    vec3 bottom = vec3(0.01, 0.02, 0.03);
    float up = clamp(n.y*0.5 + 0.5, 0.0, 1.0);
    vec3 midMix = mix(middle, top,   smoothstep(0.2, 0.9, up));
    return mix(bottom, midMix, smoothstep(0.0, 1.0, up));
}

void main(){
    vec3 N  = normalize(vNormal);
    vec3 L  = normalize(uLightDir);
    vec3 V  = normalize(uViewPos - vWorldPos);
    vec3 H  = normalize(V + L);
    float NdL = max(dot(N,L),0.0);
    float NdV = max(dot(N,V),0.0);

    // Material params
    float metallic  = 0.0;
    float roughness = (uMaterialType==1) ? 0.85 : 0.80; // rocks rougher than sand
    vec3  base = uBaseColor;

    // Rock granite-ish tint & micro normal/roughness variation
    if (uMaterialType==1){
        float speck = smoothstep(0.82, 1.0, n3(vWorldPos*18.0)) * 0.25;
        float marble = 0.5 + 0.5*sin(vWorldPos.x*8.0 + vWorldPos.z*6.3 + sin(vWorldPos.y*4.0));
        base = mix(base*0.85, base*1.10, marble) + speck*vec3(0.08,0.07,0.06);
        roughness = clamp(roughness + (n3(vWorldPos*6.0)-0.5)*0.15, 0.55, 0.95);
    } else {
        // sand: faint caustic modulation of albedo
        if (uApplyCaustics==1) base += 0.12 * caustic(vWorldPos);
    }

    vec3 F0 = mix(vec3(0.04), base, metallic);
    vec3 F  = F_Schlick(max(dot(H,V),0.0), F0);
    float D = D_GGX(N, H, roughness);
    float G = G_Smith(N, V, L, roughness);

    vec3 spec = (D * G * F) / max(4.0 * NdV * NdL + 1e-6, 1e-6);
    vec3 kd   = (1.0 - F) * (1.0 - metallic);
    vec3 diffuse = kd * base / PI;

    vec3 Lo = (diffuse + spec) * NdL;

    // Ambient
    vec3 ambient = evalAmbient(N) * kd * base;

    vec3 col = ambient + Lo;

    // Fog
    float d = distance(uViewPos, vWorldPos);
    float f = fogFactor(d);
    col = mix(uFogColor, col, f);

    FragColor = vec4(col, uAlpha);
}
