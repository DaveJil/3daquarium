#version 410 core
in vec3 vWorldPos;
in vec3 vNormal;

uniform vec3 uLightDir;   // normalized direction TO light (i.e., -sun)
uniform vec3 uViewPos;

out vec4 FragColor;

void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(-uLightDir);
    vec3 V = normalize(uViewPos - vWorldPos);
    vec3 R = reflect(-L, N);

    // simple two-tone fish with subtle stripes by height
    float stripes = 0.5 + 0.5 * sin(vWorldPos.y * 12.0);
    vec3 base = mix(vec3(0.9, 0.55, 0.2), vec3(0.2, 0.6, 0.9), stripes); // orange↔blue
    float diff = max(dot(N, L), 0.0);
    float spec = pow(max(dot(R, V), 0.0), 32.0) * 0.4;

    vec3 col = base * (0.2 + 0.8 * diff) + vec3(spec);
    FragColor = vec4(col, 1.0);
}
