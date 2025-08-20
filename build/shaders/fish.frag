#version 410 core
in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vColor;
in float vLen;
in vec3 vLocal;

uniform vec3 uLightDir;
uniform vec3 uViewPos;

uniform vec3 uFogColor;
uniform float uFogNear, uFogFar;

out vec4 FragColor;

float fogFactor(float d) {
    return clamp((uFogFar - d) / (uFogFar - uFogNear), 0.0, 1.0);
}

float hash31(vec3 p) {
    p = fract(p * 0.3183099 + vec3(0.1,0.2,0.3));
    p += dot(p, p.yzx + 19.19);
    return fract(p.x * p.y * p.z);
}

void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(-uLightDir);
    vec3 V = normalize(uViewPos - vWorldPos);
    vec3 R = reflect(-L, N);

    float diff = max(dot(N,L), 0.0);
    float spec = pow(max(dot(R,V),0.0), 64.0) * 0.25;

    // Base palette with gentle vertical tint
    float yTint = 0.25 + 0.75 * smoothstep(-0.08, 0.12, vLocal.y);
    vec3 base = mix(vColor, vec3(0.95,0.95,0.98), 0.12) * yTint;

    // Stripes along body and small speckles
    float stripes = 0.5 + 0.5 * sin(vLen * 24.0 + vLocal.y * 6.0);
    float speckle = smoothstep(0.8, 1.0, hash31(vWorldPos * 18.0));
    base = mix(base, base * vec3(0.8,0.85,1.05), stripes * 0.22);
    base += vec3(0.12,0.10,0.08) * speckle * (1.0 - vLen); // more near head

    // Rim light to accent silhouette
    float rim = pow(1.0 - max(dot(N,V),0.0), 2.0) * 0.35;

    vec3 lit = base * (0.18 + 0.82*diff) + vec3(spec) + rim*vec3(0.15,0.2,0.25);

    float d = distance(uViewPos, vWorldPos);
    float f = fogFactor(d);
    vec3 color = mix(uFogColor, lit, f);
    FragColor = vec4(color, 1.0);
}
