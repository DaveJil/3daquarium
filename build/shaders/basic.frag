#version 410 core
in vec3 vWorldPos;
in vec3 vNormal;

uniform vec3 uLightDir;   // direction TOWARDS light
uniform vec3 uViewPos;
uniform vec3 uBaseColor;
uniform vec3 uFogColor;
uniform float uFogNear, uFogFar;
uniform float uTime;
uniform int   uApplyCaustics; // 0/1
uniform float uAlpha;
uniform int   uMaterialType;  // 0=default/sand/glass, 1=rock

out vec4 FragColor;

float fogFactor(float d){ return clamp((uFogFar - d) / (uFogFar - uFogNear), 0.0, 1.0); }

float caustic(vec3 p){
    float t = uTime * 0.8;
    float v = 0.0;
    v += sin(dot(p.xz, vec2( 4.2, 3.6)) + t*1.8);
    v += sin(dot(p.xz, vec2(-3.4, 2.7)) - t*1.2);
    v = v*0.5 + 0.5;
    return smoothstep(0.75, 0.95, v);
}

// tiny hash noise
float n3(vec3 p){
    return fract(sin(dot(p, vec3(12.9898,78.233,37.719))) * 43758.5453);
}

void main(){
    vec3 N = normalize(vNormal);
    vec3 L = normalize(uLightDir);
    vec3 V = normalize(uViewPos - vWorldPos);
    vec3 R = reflect(-L, N);

    float diff = max(dot(N, L), 0.0);
    float spec = pow(max(dot(R, V), 0.0), 32.0) * 0.15;

    vec3 base = uBaseColor;

    if (uMaterialType == 1) {
        // ROCK: simple procedural granite: speckles + marbling
        float speck = smoothstep(0.82, 1.0, n3(vWorldPos*18.0)) * 0.25;
        float marble = 0.5 + 0.5*sin(vWorldPos.x*8.0 + vWorldPos.z*6.3 + sin(vWorldPos.y*4.0));
        base = mix(base*0.85, base*1.10, marble) + speck*vec3(0.08,0.07,0.06);
    }

    if (uApplyCaustics == 1)
        base += 0.15 * caustic(vWorldPos);

    vec3 lit = base * (0.2 + 0.8*diff) + vec3(spec);
    float d = distance(uViewPos, vWorldPos);
    float f = fogFactor(d);
    vec3 color = mix(uFogColor, lit, f);
    FragColor = vec4(color, uAlpha);
}
