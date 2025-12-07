#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTex;

out vec2 TexCoord;

uniform float uScaleX; 
uniform float uOffsetX; 

void main() {
    TexCoord = vec2(aTex.x * uScaleX + uOffsetX, aTex.y);
    gl_Position = vec4(aPos, 0.0, 1.0);
}
