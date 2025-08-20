#version 410 core
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uHDR;     // linear HDR color (RGBA16F)
uniform float uExposure;    // typical 0.8 .. 1.6

// ACES filmic curve (Narkowicz 2015)
vec3 ACESFilm(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x*(a*x + b)) / (x*(c*x + d) + e), 0.0, 1.0);
}

void main() {
    vec3 hdr = texture(uHDR, vUV).rgb;
    vec3 mapped = ACESFilm(hdr * uExposure);
    // IMPORTANT: output is **linear**, GL_FRAMEBUFFER_SRGB converts to sRGB on write.
    FragColor = vec4(mapped, 1.0);
}
