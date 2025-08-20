#version 410 core
out vec2 vUV;

void main() {
    // screen-space triangle (no VBO). gl_VertexID: 0,1,2
    vec2 verts[3] = vec2[3]( vec2(-1.0,-1.0), vec2(3.0,-1.0), vec2(-1.0,3.0) );
    vec2 pos = verts[gl_VertexID];
    vUV = pos * 0.5 + 0.5; // to 0..1
    gl_Position = vec4(pos, 0.0, 1.0);
}
