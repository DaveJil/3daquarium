#version 410 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;

uniform mat4 uProj, uView, uModel;

out vec3 vWorldPos;
out vec3 vNormal;

void main(){
    vec4 w = uModel * vec4(aPos,1.0);
    vWorldPos = w.xyz;
    vNormal = normalize(mat3(uModel) * aNormal);
    gl_Position = uProj * uView * w;
}
