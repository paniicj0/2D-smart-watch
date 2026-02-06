#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTex;

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vTex;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

void main(){
    vec4 wpos = uModel * vec4(aPos, 1.0);
    vWorldPos = wpos.xyz;
    vNormal = mat3(uModel) * aNormal;
    vTex = aTex;
    gl_Position = uProj * uView * wpos;
}
