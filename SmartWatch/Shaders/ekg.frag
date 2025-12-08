#version 330 core
in vec2 TexCoord;
out vec4 FragColor; //boja piksela koju sejder crta

uniform sampler2D uTexture;     //slika teksture
uniform float uGlobalAlpha;  //providnost 

void main() {
    vec4 tex = texture(uTexture, TexCoord);
    FragColor = vec4(tex.rgb, tex.a * uGlobalAlpha);//ako je 1 originalna providnost, ako je 0 potpuno neprozirno
}
