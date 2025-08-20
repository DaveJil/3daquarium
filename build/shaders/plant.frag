#version 410 core
in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vColor;

uniform vec3 uLightDir;
uniform vec3 uViewPos;

uniform vec3 uFogColor;
uniform float uFogNear, uFogFar;

out vec4 FragColor;

float fogFactor(float d) {
    return clamp((uFogFar - d) / (uFogFar - uFogNear), 0.0, 1.0);
}

void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(-uLightDir);
    vec3 V = normalize(uViewPos - vWorldPos);
    vec3 R = reflect(-L, N);

    float diff = max(dot(N,L), 0.0);
    float spec = pow(max(dot(R,V),0.0), 16.0) * 0.1;

    vec3 base = vColor;
    vec3 lit = base * (0.25 + 0.75*diff) + vec3(spec)*0.4;

    float d = distance(uViewPos, vWorldPos);
    float f = fogFactor(d);
    vec3 color = mix(uFogColor, lit, f);
    FragColor = vec4(color, 1.0);
}
