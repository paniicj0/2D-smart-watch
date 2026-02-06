#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>

// 3D wrapper oko postojeceg 2D ekrana (render u teksturu)

void init3D(GLFWwindow* window, int winW, int winH);
void updateAndRender3D(GLFWwindow* window);
void cleanup3D();
