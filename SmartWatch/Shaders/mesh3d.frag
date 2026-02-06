#version 330 core

in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vTex;

out vec4 FragColor;

uniform sampler2D uTex0;
uniform int uUseTexture; // 0/1

uniform vec3 uBaseColor;
uniform float uEmissive; // 0..1

// light
uniform vec3 uLightDir;  // directional (world)
uniform vec3 uLightColor;
uniform vec3 uViewPos;

// watch screen weak point light
uniform vec3 uScreenLightPos;
uniform vec3 uScreenLightColor;
uniform float uScreenLightRadius;

void main(){
    vec3 N = normalize(vNormal);
    vec3 base = uBaseColor;
    if(uUseTexture==1){
        base *= texture(uTex0, vTex).rgb;
    }

    vec3 L = normalize(-uLightDir);
    float diff = max(dot(N, L), 0.0);

    // weak point light from screen
    vec3 toS = uScreenLightPos - vWorldPos;
    float d = length(toS);
    float att = clamp(1.0 - d / uScreenLightRadius, 0.0, 1.0);
    vec3 Ls = d > 0.0001 ? normalize(toS) : vec3(0,1,0);
    float diffS = max(dot(N, Ls), 0.0) * att;

    vec3 ambient = 0.15 * base;
    vec3 color = ambient + (diff * uLightColor) * base + (diffS * uScreenLightColor) * base;

    // emissive (for screen)
    color += uEmissive * base;

    FragColor = vec4(color, 1.0);
}
