#include "App.h"
#include "Watch3D.h"

#include <iostream>

int main() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }

    // OpenGL 3.3 Core
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // fullscreen kao pre
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);

    GLFWwindow* window = glfwCreateWindow(mode->width, mode->height, "SmartWatch 3D", monitor, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    glfwSwapInterval(1);
    glViewport(0, 0, mode->width, mode->height);

    // inicijalizuj 2D ekran (koristi se kao tekstura na satu)
    initGL();
    initHeartCursor(window);

    // inicijalizuj 3D scenu
    init3D(window, mode->width, mode->height);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        updateAndRender3D(window);
        glfwSwapBuffers(window);
    }

    cleanup3D();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
