#version 330 core
in vec3 vNormal;
in vec3 vWorldPos;

uniform vec3 uLightDir; // directional light (normalized)
uniform vec3 uViewPos;
uniform vec3 uColor;

out vec4 FragColor;

void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(-uLightDir);
    vec3 V = normalize(uViewPos - vWorldPos);
    vec3 R = reflect(-L, N);

    float diff = max(dot(N,L), 0.0);
    float spec = pow(max(dot(R,V),0.0), 32.0);

    vec3 base = uColor;
    vec3 col = base * (0.15 + 0.85*diff) + vec3(0.6)*spec;
    FragColor = vec4(col, 1.0);
}
