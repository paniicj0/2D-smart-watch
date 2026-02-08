#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTex;

out vec3 vWorldPos;//distanca od kamere do povrsine, potrebna za izracunavanje osvjetljenja
out vec3 vNormal;
out vec2 vTex;

uniform mat4 uModel;//transformacija
uniform mat4 uView;//kamera
uniform mat4 uProj;//projekcija

void main(){
    vec4 wpos = uModel * vec4(aPos, 1.0);
    vWorldPos = wpos.xyz;
    vNormal = mat3(transpose(inverse(uModel))) * aNormal;//transformacija normale u isti prostor kao i svetlo
    vTex = aTex;
    gl_Position = uProj * uView * wpos;
}
