#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTex;

out vec2 TexCoord;

uniform float uScaleX; //koliko je ekg zbijen
uniform float uOffsetX; //koliko je ekg pomeren levo

void main() {
    TexCoord = vec2(aTex.x * uScaleX + uOffsetX, aTex.y);//zgusnjavanje i pomeranje teksture
    gl_Position = vec4(aPos, 0.0, 1.0);
}
