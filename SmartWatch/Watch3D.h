#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>


void init3D(GLFWwindow* window, int winW, int winH);
void updateAndRender3D(GLFWwindow* window);
void cleanup3D();
