#version 410 core
in vec3 vWorldPos;
in vec3 vNormal;

uniform vec3 uLightDir;
uniform vec3 uViewPos;
uniform vec3 uBaseColor;
uniform vec3 uFogColor;
uniform float uFogNear, uFogFar;
uniform float uTime;

uniform int   uApplyCaustics;
uniform float uAlpha;
uniform int   uMaterialType;  // 0=sand/glass, 1=rock

// IBL
uniform samplerCube uIrradiance;
uniform samplerCube uPrefilter;
uniform sampler2D   uBRDFLUT;
uniform float       uPrefLodMax;

out vec4 FragColor;

const float PI = 3.14159265359;

float fogFactor(float d){ return clamp((uFogFar - d) / (uFogFar - uFogNear), 0.0, 1.0); }
float sat(float x){ return clamp(x,0.0,1.0); }

vec3 F_Schlick(float cosTheta, vec3 F0){ return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0); }
float n3(vec3 p){ return fract(sin(dot(p, vec3(12.9898,78.233,37.719))) * 43758.5453); }
float caustic(vec3 p){
    float t = uTime * 0.8;
    float v = 0.0;
    v += sin(dot(p.xz, vec2( 4.2, 3.6)) + t*1.8);
    v += sin(dot(p.xz, vec2(-3.4, 2.7)) - t*1.2);
    v = v*0.5 + 0.5;
    return smoothstep(0.75, 0.95, v);
}

void main(){
    vec3 N  = normalize(vNormal);
    vec3 L  = normalize(uLightDir);
    vec3 V  = normalize(uViewPos - vWorldPos);
    vec3 H  = normalize(V + L);
    float NdL = max(dot(N,L),0.0);
    float NdV = max(dot(N,V),0.0);

    float metallic  = 0.0;
    float roughness = (uMaterialType==1) ? 0.85 : 0.80;
    vec3  base = uBaseColor;

    if (uMaterialType==1){
        float speck = smoothstep(0.82, 1.0, n3(vWorldPos*18.0)) * 0.25;
        float marble = 0.5 + 0.5*sin(vWorldPos.x*8.0 + vWorldPos.z*6.3 + sin(vWorldPos.y*4.0));
        base = mix(base*0.85, base*1.10, marble) + speck*vec3(0.08,0.07,0.06);
        roughness = clamp(roughness + (n3(vWorldPos*6.0)-0.5)*0.15, 0.55, 0.95);
    } else {
        if (uApplyCaustics==1) base += 0.12 * caustic(vWorldPos);
    }

    vec3 F0 = mix(vec3(0.04), base, metallic);
    vec3 F  = F_Schlick(max(dot(H,V),0.0), F0);
    float a = roughness*roughness, a2=a*a;
    float NdH = max(dot(N,H),0.0);
    float denom = NdH*NdH*(a2-1.0)+1.0;
    float D = a2 / max(PI*denom*denom, 1e-6);
    float r = roughness+1.0, k=(r*r)/8.0;
    float G = (NdV/(NdV*(1.0-k)+k)) * (NdL/(NdL*(1.0-k)+k));
    vec3  spec = (D * G * F) / max(4.0 * NdV * NdL + 1e-6, 1e-6);
    vec3  kd   = (1.0 - F) * (1.0 - metallic);
    vec3  Lo   = (kd*base/PI + spec) * NdL;

    // IBL
    vec3 diffuseIBL = texture(uIrradiance, N).rgb * kd * base;
    vec3 R = reflect(-V, N);
    float lod = roughness * uPrefLodMax;
    vec3 prefiltered = textureLod(uPrefilter, R, lod).rgb;
    vec2 brdf = texture(uBRDFLUT, vec2(NdV, roughness)).rg;
    vec3 specIBL = prefiltered * (F * brdf.x + brdf.y);

    vec3 col = diffuseIBL + specIBL + Lo;

    float d = distance(uViewPos, vWorldPos);
    float f = fogFactor(d);
    col = mix(uFogColor, col, f);

    FragColor = vec4(col, uAlpha);
}
