#version 410 core
in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vColor;
in float vLen;    // along body 0..1 (nose..tail)
in vec3 vLocal;   // local animated space
in float vSpecies;

uniform vec3 uLightDir;  // direction TOWARDS light
uniform vec3 uViewPos;

uniform vec3 uFogColor;
uniform float uFogNear, uFogFar;
uniform float uTime;

out vec4 FragColor;

float fogFactor(float d){ return clamp((uFogFar - d) / (uFogFar - uFogNear), 0.0, 1.0); }
float sat(float x){ return clamp(x,0.0,1.0); }

// soft stripes helper along body
float stripe(float x, float c, float w){
    return smoothstep(c-w, c-0.5*w, x) - smoothstep(c+0.5*w, c+w, x);
}

float hash31(vec3 p){
    p = fract(p * 0.3183099 + vec3(0.1,0.2,0.3));
    p += dot(p, p.yzx + 19.19);
    return fract(p.x * p.y * p.z);
}

void main(){
    vec3 N = normalize(vNormal);
    vec3 L = normalize(uLightDir);
    vec3 V = normalize(uViewPos - vWorldPos);
    vec3 R = reflect(-L, N);

    float diff = max(dot(N,L), 0.0);
    float spec = pow(max(dot(R,V),0.0), 64.0) * 0.25;

    // base palette per species
    vec3 base;
    if (int(vSpecies+0.5) == 0) {
        // Clownfish (Nemo): orange + white bands with black borders
        base = mix(vec3(1.0,0.55,0.18), vColor, 0.25);
        float band1 = stripe(vLen, 0.18, 0.07);
        float band2 = stripe(vLen, 0.47, 0.07);
        float band3 = stripe(vLen, 0.76, 0.06);
        float bands = max(band1, max(band2, band3));
        // black border by widening slightly
        float border = max(stripe(vLen,0.18,0.11), max(stripe(vLen,0.47,0.11), stripe(vLen,0.76,0.10))) - bands;
        vec3 orange = base;
        vec3 white  = vec3(0.95);
        vec3 black  = vec3(0.04);
        base = mix(orange, white, bands);
        base = mix(base, black, sat(border)*0.85);
    }
    else if (int(vSpecies+0.5) == 1) {
        // Neon tetra: cyan/blue iridescent stripe + red belly
        float y = vLocal.y; // vertical
        vec3 top  = vec3(0.2,0.95,1.0);
        vec3 bot  = vec3(0.9,0.18,0.2);
        float split = smoothstep(-0.03, 0.02, y);
        base = mix(bot, top, split);
        // iridescence with view angle
        float iri = pow(1.0 - max(dot(N,V),0.0), 3.0);
        base += vec3(0.15,0.2,0.25) * iri;
    }
    else {
        // Zebra danio: blue/gold longitudinal stripes
        float stripes = 0.6 + 0.4*sin(vLocal.y*40.0 + sin(vLen*15.0)*0.5);
        vec3 gold = vec3(0.95,0.85,0.55);
        vec3 blue = vec3(0.25,0.45,0.85);
        base = mix(blue, gold, stripes*0.5);
    }

    // subtle speckles near head for realism
    float speckle = smoothstep(0.82, 1.0, hash31(vWorldPos*18.0));
    base += vec3(0.08,0.07,0.06) * speckle * (1.0 - vLen);

    // Rim light for silhouette
    float rim = pow(1.0 - max(dot(N,V),0.0), 2.0) * 0.35;

    // Eye: near head (vLen ~ 0.07), circular on the side (|z| high)
    float eyeCenter = 0.075;
    float eye = smoothstep(0.028, 0.018, length(vec2((vLen - eyeCenter)*1.8, abs(vLocal.z) - 0.02)));
    float pupil = smoothstep(0.012, 0.008, length(vec2((vLen - eyeCenter)*1.8, abs(vLocal.z) - 0.02)));
    vec3 eyeCol = mix(vec3(0.1), vec3(0.95), eye) * (1.0 - pupil) + vec3(0.02) * pupil;
    // specular glint on eye
    float glint = smoothstep(0.016, 0.0, length(vec2((vLen - (eyeCenter-0.01))*1.8, (abs(vLocal.z)-0.03))));
    eyeCol += vec3(1.0) * glint * 0.25;

    vec3 lit = base * (0.18 + 0.82*diff) + vec3(spec) + rim*vec3(0.15,0.2,0.25);
    lit = mix(lit, eyeCol, eye); // overlay eye region

    float d = distance(uViewPos, vWorldPos);
    float f = fogFactor(d);
    vec3 color = mix(uFogColor, lit, f);
    FragColor = vec4(color, 1.0);
}
