#version 410 core
in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vScreenUV;

uniform sampler2D uSceneColor; // HDR opaque scene
uniform vec3 uDeepColor;
uniform vec3 uShallowColor;
uniform vec3 uLightDir;
uniform vec3 uViewPos;
uniform float uTime;

out vec4 FragColor;

float fresnel(vec3 n, vec3 v){
    float f0 = 0.02;
    float cosTheta = clamp(dot(n, v), 0.0, 1.0);
    return f0 + (1.0 - f0)*pow(1.0 - cosTheta, 5.0);
}

void main(){
    vec3 N = normalize(vNormal);
    vec3 V = normalize(uViewPos - vWorldPos);
    vec3 L = normalize(uLightDir);

    float distortAmt = 0.03;
    vec2 distort = N.xz * distortAmt;

    vec3 refracted = texture(uSceneColor, vScreenUV + distort).rgb;

    float depthTint = clamp((vWorldPos.y + 1.2) * 0.6, 0.0, 1.0);
    vec3 waterTint = mix(uDeepColor, uShallowColor, depthTint);
    refracted = mix(refracted, waterTint, 0.25);

    float spec = pow(max(dot(reflect(-L,N), V), 0.0), 128.0) * 0.5;

    float F = fresnel(N, V);
    vec3 reflectedApprox = mix(vec3(0.03,0.06,0.09), waterTint*1.3, 0.6);

    vec3 color = mix(refracted, reflectedApprox, F) + spec;

    float foam = smoothstep(0.02, 0.06, length(N.xz));
    color += foam * 0.08;

    FragColor = vec4(color, 0.62);
}
