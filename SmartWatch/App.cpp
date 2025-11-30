#include "App.h"

// === Definicije globalnih promenljivih ===
Screen currentScreen = Screen::TIME;
bool leftMouseDownLastFrame = false;

GLuint quadVAO = 0;
GLuint quadVBO = 0;
GLuint shaderProgram = 0;

// Dugmi?i (strelice) – NDC koordinate
Button arrowRightTime{ 0.6f, 0.9f, -0.1f, 0.1f };   // TIME -> HEART
Button arrowLeftHeart{ -0.9f, -0.6f, -0.1f, 0.1f }; // HEART -> TIME
Button arrowRightHeart{ 0.6f, 0.9f, -0.1f, 0.1f };   // HEART -> BATTERY
Button arrowLeftBattery{ -0.9f, -0.6f, -0.1f, 0.1f }; // BATTERY -> HEART


// === GLSL šejderi za 2D pravougaonik ===
const char* vertexShaderSrc = R"(
#version 330 core
layout (location = 0) in vec2 aPos;

void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)";

const char* fragmentShaderSrc = R"(
#version 330 core
out vec4 FragColor;
uniform vec3 uColor;

void main() {
    FragColor = vec4(uColor, 1.0);
}
)";


// === Pomo?ne funkcije (samo u ovom fajlu) ===
static GLuint compileShader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    // (po želji dodaj check za greške)
    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "Shader compile error: " << infoLog << std::endl;
    }

    return shader;
}

static void drawQuad(float xMin, float xMax, float yMin, float yMax,
    float r, float g, float b) {
    float vertices[12] = {
        xMin, yMin,
        xMax, yMin,
        xMax, yMax,

        xMin, yMin,
        xMax, yMax,
        xMin, yMax
    };

    glUseProgram(shaderProgram);

    GLint colorLoc = glGetUniformLocation(shaderProgram, "uColor");
    glUniform3f(colorLoc, r, g, b);

    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    glBindVertexArray(0);
}


// === initGL: poziva se jednom iz main() ===
void initGL() {
    // blending za providne teksture (treba?e kasnije)
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // kompajliraj šejdere i napravi program
    GLuint vs = compileShader(GL_VERTEX_SHADER, vertexShaderSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSrc);

    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vs);
    glAttachShader(shaderProgram, fs);
    glLinkProgram(shaderProgram);

    glDeleteShader(vs);
    glDeleteShader(fs);

    // VAO / VBO za pravougaonik
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);

    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, 6 * 2 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

    glBindVertexArray(0);
}


// === updateAndRender: jedan frame (input + logika + crtanje) ===
void updateAndRender(GLFWwindow* window) {
    // --- 1) input ---
    glfwPollEvents();

    // ESC gasi aplikaciju
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }

    // dimenzije prozora (za konverziju miša u NDC)
    int windowWidth, windowHeight;
    glfwGetFramebufferSize(window, &windowWidth, &windowHeight);

    // detekcija klika (edge – samo trenutak kada je kliknut)
    int mouseState = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
    bool justClicked = (mouseState == GLFW_PRESS && !leftMouseDownLastFrame);
    leftMouseDownLastFrame = (mouseState == GLFW_PRESS);

    if (justClicked) {
        double mx, my;
        glfwGetCursorPos(window, &mx, &my);

        float mouseXndc = static_cast<float>((mx / windowWidth) * 2.0 - 1.0);
        float mouseYndc = static_cast<float>(1.0 - (my / windowHeight) * 2.0);

        auto inside = [&](const Button& b) {
            return mouseXndc >= b.xMin && mouseXndc <= b.xMax &&
                mouseYndc >= b.yMin && mouseYndc <= b.yMax;
            };

        switch (currentScreen) {
        case Screen::TIME:
            if (inside(arrowRightTime)) {
                currentScreen = Screen::HEART;
            }
            break;
        case Screen::HEART:
            if (inside(arrowLeftHeart)) {
                currentScreen = Screen::TIME;
            }
            else if (inside(arrowRightHeart)) {
                currentScreen = Screen::BATTERY;
            }
            break;
        case Screen::BATTERY:
            if (inside(arrowLeftBattery)) {
                currentScreen = Screen::HEART;
            }
            break;
        }
    }

    // --- 2) render ---
    switch (currentScreen) {
    case Screen::TIME:
        glClearColor(0.1f, 0.1f, 0.3f, 1.0f);
        break;
    case Screen::HEART:
        glClearColor(0.3f, 0.0f, 0.0f, 1.0f);
        break;
    case Screen::BATTERY:
        glClearColor(0.0f, 0.25f, 0.0f, 1.0f);
        break;
    }

    glClear(GL_COLOR_BUFFER_BIT);

    // strelice (za sad kao beli pravougaonici)
    if (currentScreen == Screen::TIME) {
        drawQuad(arrowRightTime.xMin, arrowRightTime.xMax,
            arrowRightTime.yMin, arrowRightTime.yMax,
            1.0f, 1.0f, 1.0f);
    }
    else if (currentScreen == Screen::HEART) {
        drawQuad(arrowLeftHeart.xMin, arrowLeftHeart.xMax,
            arrowLeftHeart.yMin, arrowLeftHeart.yMax,
            1.0f, 1.0f, 1.0f);

        drawQuad(arrowRightHeart.xMin, arrowRightHeart.xMax,
            arrowRightHeart.yMin, arrowRightHeart.yMax,
            1.0f, 1.0f, 1.0f);
    }
    else if (currentScreen == Screen::BATTERY) {
        drawQuad(arrowLeftBattery.xMin, arrowLeftBattery.xMax,
            arrowLeftBattery.yMin, arrowLeftBattery.yMax,
            1.0f, 1.0f, 1.0f);
    }
}
