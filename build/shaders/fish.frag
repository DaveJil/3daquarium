#version 410 core
in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vColor;
in float vLen;
in vec3 vLocal;
in float vSpecies;
in vec3 vTanU;
in vec3 vTanV;

in float vFinMask;
in vec2  vFinUV;

uniform vec3 uLightDir;
uniform vec3 uViewPos;
uniform vec3 uFogColor;
uniform float uFogNear, uFogFar;
uniform float uTime;

// IBL
uniform samplerCube uIrradiance;
uniform samplerCube uPrefilter;
uniform sampler2D   uBRDFLUT;
uniform float       uPrefLodMax;

out vec4 FragColor;

const float PI = 3.14159265359;

// helpers
float fogFactor(float d){ return clamp((uFogFar - d) / (uFogFar - uFogNear), 0.0, 1.0); }
float sat(float x){ return clamp(x,0.0,1.0); }
vec3 F_Schlick(float cosTheta, vec3 F0){ return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0); }

// palettes
float stripe(float x, float c, float w){
    return smoothstep(c-w, c-0.5*w, x) - smoothstep(c+0.5*w, c+w, x);
}
vec3 speciesBaseColor(int sp, float len, vec3 local, vec3 tint){
    if (sp == 0) {
        vec3 orange = mix(vec3(1.0,0.55,0.18), tint, 0.25);
        float band1 = stripe(len, 0.18, 0.07);
        float band2 = stripe(len, 0.47, 0.07);
        float band3 = stripe(len, 0.76, 0.06);
        float bands = max(band1, max(band2, band3));
        float border = max(stripe(len,0.18,0.11), max(stripe(len,0.47,0.11), stripe(len,0.76,0.10))) - bands;
        vec3 white  = vec3(0.95);
        vec3 black  = vec3(0.04);
        vec3 base = mix(orange, white, bands);
        return mix(base, black, sat(border)*0.85);
    } else if (sp == 1) {
        float y = local.y;
        vec3 top  = vec3(0.2,0.95,1.0);
        vec3 bot  = vec3(0.9,0.18,0.2);
        float split = smoothstep(-0.03, 0.02, y);
        vec3 base = mix(bot, top, split);
        float iri = pow(1.0 - max(dot(normalize(vNormal), normalize(uViewPos - (vWorldPos))),0.0), 3.0);
        return base + vec3(0.15,0.2,0.25) * iri;
    } else {
        float s = 0.6 + 0.4*sin(local.y*40.0 + sin(len*15.0)*0.5);
        vec3 gold = vec3(0.95,0.85,0.55);
        vec3 blue = vec3(0.25,0.45,0.85);
        return mix(blue, gold, s*0.5);
    }
}

// Worley “scale cells” bump for BODY
vec2 hash22(vec2 p){
    p = vec2(dot(p, vec2(127.1,311.7)), dot(p, vec2(269.5,183.3)));
    return fract(sin(p)*43758.5453);
}
vec2 worleyF1F2(vec2 uv, float cells, out vec2 jRand){
    uv *= cells;
    vec2 i = floor(uv);
    vec2 f = fract(uv);
    float F1 = 1e9, F2 = 1e9; vec2 bestR = vec2(0.0);
    for (int y=-1;y<=1;y++) for (int x=-1;x<=1;x++){
        vec2 g = vec2(x,y);
        vec2 o = hash22(i+g)-0.5;
        vec2 d = g + o - f;
        float dist = dot(d,d);
        if (dist < F1){ F2 = F1; F1 = dist; bestR = o; }
        else if (dist < F2){ F2 = dist; }
    }
    jRand = bestR; return vec2(sqrt(F1), sqrt(F2));
}
float scaleHeight(vec2 uv, float density, float edgeW){
    vec2 j; vec2 f = worleyF1F2(uv, density, j);
    float edge = clamp(f.y - f.x, 0.0, 1.0);
    float ridge = 1.0 - smoothstep(edgeW, edgeW*1.8, edge);
    return ridge;
}

// Fin normal map (procedural)
float finHeightFn(vec2 uv){
    float t = uTime;
    float ridges = sin(uv.x*7.5 + sin(uv.y*1.8 + t*2.0)*0.8)
                 + 0.35*sin(uv.x*14.0 + t*1.7);
    ridges += 0.25*sin(uv.y*10.0 + sin(uv.x*2.5)*0.7);
    return ridges * 0.5;
}
vec3 finWorldNormal(vec3 N0, vec3 T, vec3 B, vec2 uv){
    float eps = 0.003;
    float h  = finHeightFn(uv);
    float hx = finHeightFn(uv + vec2(eps,0.0)) - h;
    float hy = finHeightFn(uv + vec2(0.0,eps)) - h;
    float amp = 0.35;
    vec3 n_ts = normalize(vec3(-hx*amp, -hy*amp, 1.0));
    return normalize(T*n_ts.x + B*n_ts.y + N0*n_ts.z);
}

void main(){
    vec3 N0 = normalize(vNormal);
    vec3 L  = normalize(uLightDir);
    vec3 V  = normalize(uViewPos - vWorldPos);

    float u = clamp(vLen, 0.0, 1.0);
    float theta = atan(vLocal.z, vLocal.y);
    float vv = (theta / 6.2831853) + 0.5;
    float density = mix(52.0, 28.0, smoothstep(0.05, 0.8, u));
    vec2  bodyUV = vec2(u, vv * mix(1.6, 1.25, u));

    float h  = scaleHeight(bodyUV, density, 0.055);
    float eps = 0.003;
    float hu = scaleHeight(bodyUV + vec2(eps,0.0), density, 0.055) - h;
    float hv = scaleHeight(bodyUV + vec2(0.0,eps), density, 0.055) - h;

    float bumpK = 0.65 * smoothstep(0.06, 0.25, u);
    bumpK *= (1.0 - vFinMask);

    vec3 T = normalize(vTanU);
    vec3 B = normalize(vTanV);
    vec3 N_body = normalize(N0 + bumpK * (hu*T + hv*B));
    vec3 N_fin  = finWorldNormal(N0, T, B, vFinUV);
    vec3 N = normalize( mix(N_body, N_fin, vFinMask) );

    vec3 base = speciesBaseColor(int(vSpecies+0.5), u, vLocal, vColor);

    // Eye
    float eyeCenter = 0.075;
    float eye = smoothstep(0.028, 0.018, length(vec2((u - eyeCenter)*1.8, abs(vLocal.z) - 0.02)));
    float pupil = smoothstep(0.012, 0.008, length(vec2((u - eyeCenter)*1.8, abs(vLocal.z) - 0.02)));
    vec3 eyeCol = mix(vec3(0.1), vec3(0.95), eye) * (1.0 - pupil) + vec3(0.02) * pupil;

    // PBR params
    float metallic  = 0.0;
    float roughBody = 0.35;
    float roughFin  = 0.55;
    float roughness = mix(roughBody, roughFin, vFinMask);

    vec3  F0 = mix(vec3(0.04), base, metallic);

    // Direct light (single dir)
    vec3  H = normalize(V + L);
    float NdL = max(dot(N, L), 0.0);
    float NdV = max(dot(N, V), 0.0);
    vec3  F  = F_Schlick(max(dot(H,V),0.0), F0);
    float a  = roughness*roughness;
    float a2 = a*a;
    float NdH = max(dot(N,H),0.0);
    float denom = NdH*NdH*(a2-1.0)+1.0;
    float D = a2 / max(PI*denom*denom, 1e-6);
    float r = roughness+1.0;
    float k = (r*r)/8.0;
    float G = (NdV / (NdV*(1.0-k)+k)) * (NdL / (NdL*(1.0-k)+k));
    vec3  spec = (D * G * F) / max(4.0 * NdV * NdL + 1e-6, 1e-6);
    vec3  kd   = (1.0 - F) * (1.0 - metallic);
    vec3  Lo   = (kd*base/PI + spec) * NdL;

    // IBL
    vec3 irradiance = texture(uIrradiance, N).rgb;
    vec3 diffuseIBL = irradiance * kd * base;

    vec3 R = reflect(-V, N);
    float lod = roughness * uPrefLodMax;
    vec3 prefiltered = textureLod(uPrefilter, R, lod).rgb;
    vec2 brdf = texture(uBRDFLUT, vec2(NdV, roughness)).rg;
    vec3 specIBL = prefiltered * (F * brdf.x + brdf.y);

    // Body sparkle (tiny carry-over)
    float sparkle = pow(max(dot(reflect(-L, N), normalize(V)), 0.0), 170.0) * (0.12 * h) * (1.0 - vFinMask);

    // SSS for thin parts
    float backLight = pow(max(dot(-L, vNormal), 0.0), 1.6);
    float belly = smoothstep(0.00, 0.18, -vLocal.y) * (1.0 - smoothstep(0.65, 0.95, u));
    float thinMask = clamp(belly*0.8 + vFinMask*1.0, 0.0, 1.0);
    vec3  sssCol = mix(base, vec3(1.0, 0.86, 0.7), 0.55);
    vec3  sss = sssCol * backLight * (0.45 * thinMask);

    vec3 lit = diffuseIBL + specIBL + Lo + vec3(sparkle) + sss;

    // eye on top
    lit = mix(lit, eyeCol, eye);

    // Fog
    float dist = distance(uViewPos, vWorldPos);
    float f = fogFactor(dist);
    vec3 color = mix(uFogColor, lit, f);

    FragColor = vec4(color, 1.0);
}
