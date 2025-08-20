#version 410 core
in vec3 vWorldPos;
in float vFoam;

uniform vec3 uDeepColor;
uniform vec3 uShallowColor;

out vec4 FragColor;

void main() {
    float t = clamp((vWorldPos.y + 1.0) * 0.5, 0.0, 1.0);
    vec3 water = mix(uDeepColor, uShallowColor, t);
    water += vec3(vFoam) * 0.12;
    FragColor = vec4(water, 0.60); // clear, blended
}
