#version 330 core
out vec4 FragColor; //konacna boj asvakog piksela
uniform vec3 uColor;

void main() {
    FragColor = vec4(uColor, 1.0); //boja svakog piksela jednaka boji iz programa
}
