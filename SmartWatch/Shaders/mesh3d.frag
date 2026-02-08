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
uniform vec3 uLightDir;  // smer kuda svetlo ide (odozgo)
uniform vec3 uLightColor;
uniform vec3 uViewPos;

// watch screen weak point light
uniform vec3 uScreenLightPos;
uniform vec3 uScreenLightColor;
uniform float uScreenLightRadius;

void main(){
    vec3 N = normalize(vNormal);
    vec3 base = uBaseColor;
    if(uUseTexture==1){//svaki objekat ima neku boju
        base *= texture(uTex0, vTex).rgb;
    }

    vec3 L = normalize(-uLightDir);
    float diff = max(dot(N, L), 0.0);

   
    vec3 toS = uScreenLightPos - vWorldPos;//vektor od piksela do ekrana 
    float d = length(toS);
    float att = clamp(1.0 - d / uScreenLightRadius, 0.0, 1.0);//slabljenje linearno opada do 0 na radiusu
    vec3 Ls = d > 0.0001 ? normalize(toS) : vec3(0,1,0);
    float diffS = max(dot(N, Ls), 0.0) * att; //dot meri koliko je svetlo okrenuto ka povrsini, max da nema negativnog osvetljenja

    vec3 ambient = 0.19 * base;//stalno malo svetla da ne bude crno u senci
    vec3 color = ambient + (diff * uLightColor) * base + (diffS * uScreenLightColor) * base;

    // da ekran ima svoju svetlost, cak i kad bi bio mrak
    color += uEmissive * base;

    FragColor = vec4(color, 1.0);//sve je neprovidno
}
