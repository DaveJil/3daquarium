#version 410 core
in vec3 vNormal;
in vec3 vWorldPos;

uniform vec3 uLightDir;   // normalized direction to light
uniform vec3 uViewPos;
uniform vec3 uBaseColor;
uniform float uAlpha;

uniform vec3 uFogColor;
uniform float uFogNear, uFogFar;
uniform float uTime;
uniform int   uApplyCaustics;

out vec4 FragColor;

float fogFactor(float d) {
    return clamp((uFogFar - d) / (uFogFar - uFogNear), 0.0, 1.0);
}

float caustics(vec2 p, float t) {
    float c = 0.0;
    c += sin(dot(p, vec2(12.1, 7.3)) + t*1.3);
    c += sin(dot(p, vec2(-9.7, 5.5)) - t*1.6);
    c += sin(dot(p, vec2(6.3, -11.4)) + t*1.1);
    return (c/3.0)*0.5 + 0.5; // 0..1
}

void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(-uLightDir);
    vec3 V = normalize(uViewPos - vWorldPos);
    vec3 R = reflect(-L, N);

    float diff = max(dot(N,L), 0.0);
    float spec = pow(max(dot(R,V),0.0), 32.0) * 0.4;

    vec3 base = uBaseColor;
    if (uApplyCaustics == 1) {
        float c = caustics(vWorldPos.xz*0.9, uTime);
        base *= 0.85 + 0.35*c;
    }
    vec3 lit = base * (0.15 + 0.85*diff) + vec3(spec);

    float d = distance(uViewPos, vWorldPos);
    float f = fogFactor(d);
    vec3 color = mix(uFogColor, lit, f);
    FragColor = vec4(color, uAlpha);
}
