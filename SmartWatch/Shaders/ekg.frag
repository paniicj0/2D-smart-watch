#version 330 core
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D uTexture;
uniform float uGlobalAlpha;   

void main() {
    vec4 tex = texture(uTexture, TexCoord);
    FragColor = vec4(tex.rgb, tex.a * uGlobalAlpha);
}
