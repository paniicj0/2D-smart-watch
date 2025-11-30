#include <iostream>
#include <thread>
#include <chrono>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

int main() {
    // 1) Init GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }

    // OpenGL 3.3 Core profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // 2) Fullscreen na primarnom monitoru
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);

    if (!monitor || !mode) {
        std::cerr << "Failed to get primary monitor or video mode\n";
        glfwTerminate();
        return -1;
    }

    GLFWwindow* window = glfwCreateWindow(
        mode->width,
        mode->height,
        "2D Smart Watch",
        monitor,    // fullscreen
        nullptr
    );

    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    // Isključimo VSync, sami ćemo ograničiti FPS na 75
    glfwSwapInterval(0);

    // 3) Init GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    // Podesi viewport (ceo ekran)
    glViewport(0, 0, mode->width, mode->height);

    // Target: 75 FPS -> ~13.333 ms po frejmu
    const double targetFrameTime = 1.0 / 75.0;

    // 4) Glavna petlja
    while (!glfwWindowShouldClose(window)) {
        double frameStart = glfwGetTime();

        // ESC za izlaz
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, true);
        }

        // crna pozadina
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // >>> OVDE ĆE POSLE IĆI CRTANJE EKRANA PAMETNOG SATA <<<

        glfwSwapBuffers(window);
        glfwPollEvents();

        // Frame limiter na ~75 FPS
        double frameEnd = glfwGetTime();
        double frameDuration = frameEnd - frameStart;

        if (frameDuration < targetFrameTime) {
            double remaining = targetFrameTime - frameDuration;
            std::this_thread::sleep_for(
                std::chrono::duration<double>(remaining)
            );
        }
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
