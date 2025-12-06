#include "App.h"
#include <chrono>
#include <thread>

int main() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }

    // OpenGL 3.3 Core profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Fullscreen 
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
	const GLFWvidmode* mode = glfwGetVideoMode(monitor);//trenutni parametri monitora

    GLFWwindow* window = glfwCreateWindow(mode->width, mode->height,
        "SmartWatch", monitor, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    // glad
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    // VSYNC off (zbog frame limitera)
    glfwSwapInterval(0);

	// crtaj od 0,0 do width,height
    glViewport(0, 0, mode->width, mode->height);

    initGL();
	initHeartCursor(window);

	// limiter 75 FPS
    const double TARGET_FPS = 75.0;
    const double TARGET_FRAME_TIME = 1.0 / TARGET_FPS;

    while (!glfwWindowShouldClose(window)) {
        double frameStart = glfwGetTime();

        // input + logika + crtanje
        updateAndRender(window);

        glfwSwapBuffers(window);

        double frameEnd = glfwGetTime();
        double frameTime = frameEnd - frameStart;

        if (frameTime < TARGET_FRAME_TIME) {
            double sleepTime = TARGET_FRAME_TIME - frameTime;
            while (glfwGetTime() - frameEnd < sleepTime) {
            }
        }
    }

    // ciscenje
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
