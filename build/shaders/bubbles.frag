#version 410 core
out vec4 FragColor;
void main(){
    vec2 p = gl_PointCoord * 2.0 - 1.0;
    float m = smoothstep(1.0, 0.0, dot(p,p));
    vec3 col = vec3(0.85,0.9,1.0);
    FragColor = vec4(col, m*0.9);
}
