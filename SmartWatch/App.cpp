#include "App.h"
#include <ctime>   // za lokalno vreme

// === Definicije globalnih promenljivih ===
Screen currentScreen = Screen::TIME;
bool leftMouseDownLastFrame = false;

GLuint quadVAO = 0;
GLuint quadVBO = 0;
GLuint shaderProgram = 0;

// === Sat ===
int g_hours = 0;
int g_minutes = 0;
int g_seconds = 0;

static double lastClockUpdate = 0.0;

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

// redosled segmenata: 
// 0 = gornji, 1 = gornji-desni, 2 = donji-desni,
// 3 = donji, 4 = donji-levi, 5 = gornji-levi, 6 = srednji
static const bool DIGIT_SEGMENTS[10][7] = {
    // 0
    { true,  true,  true,  true,  true,  true,  false },
    // 1
    { false, true,  true,  false, false, false, false },
    // 2
    { true,  true,  false, true,  true,  false, true  },
    // 3
    { true,  true,  true,  true,  false, false, true  },
    // 4
    { false, true,  true,  false, false, true,  true  },
    // 5
    { true,  false, true,  true,  false, true,  true  },
    // 6
    { true,  false, true,  true,  true,  true,  true  },
    // 7
    { true,  true,  true,  false, false, false, false },
    // 8
    { true,  true,  true,  true,  true,  true,  true  },
    // 9
    { true,  true,  true,  true,  false, true,  true  }
};

// crta jedan segment (pravougaonik) relativno u odnosu na centar cifre
static void drawSegment(int segIndex,
    float cx, float cy,
    float w, float h,
    float r, float g, float b)
{
    float thickness = w * 0.20f;      // debljina segmenta
    float halfW = w * 0.5f;
    float halfH = h * 0.5f;

    float xMin, xMax, yMin, yMax;

    switch (segIndex) {
    case 0: // gornji
        xMin = cx - halfW;
        xMax = cx + halfW;
        yMax = cy + halfH;
        yMin = yMax - thickness;
        break;
    case 3: // donji
        xMin = cx - halfW;
        xMax = cx + halfW;
        yMin = cy - halfH;
        yMax = yMin + thickness;
        break;
    case 6: // srednji
        xMin = cx - halfW;
        xMax = cx + halfW;
        yMin = cy - thickness * 0.5f;
        yMax = cy + thickness * 0.5f;
        break;
    case 5: // gornji-levi
        xMin = cx - halfW;
        xMax = xMin + thickness;
        yMax = cy + halfH;
        yMin = cy;
        break;
    case 4: // donji-levi
        xMin = cx - halfW;
        xMax = xMin + thickness;
        yMax = cy;
        yMin = cy - halfH;
        break;
    case 1: // gornji-desni
        xMax = cx + halfW;
        xMin = xMax - thickness;
        yMax = cy + halfH;
        yMin = cy;
        break;
    case 2: // donji-desni
        xMax = cx + halfW;
        xMin = xMax - thickness;
        yMax = cy;
        yMin = cy - halfH;
        break;
    default:
        return;
    }

    drawQuad(xMin, xMax, yMin, yMax, r, g, b);
}

// crta jednu cifru (digit) na zadatom mestu
static void drawDigit(int digit,
    float cx, float cy,
    float w, float h,
    float r, float g, float b)
{
    if (digit < 0 || digit > 9) return;

    for (int s = 0; s < 7; ++s) {
        if (DIGIT_SEGMENTS[digit][s]) {
            drawSegment(s, cx, cy, w, h, r, g, b);
        }
    }
}

// dve tačkice ':' između HH i MM, i MM i SS
static void drawColon(float cx, float cy, float size,
    float r, float g, float b)
{
    float dotH = size * 0.2f;
    float dotW = size * 0.15f;
    float offsetY = size * 0.25f;

    // gornja tačka
    drawQuad(cx - dotW, cx + dotW,
        cy + offsetY - dotH, cy + offsetY + dotH,
        r, g, b);

    // donja tačka
    drawQuad(cx - dotW, cx + dotW,
        cy - offsetY - dotH, cy - offsetY + dotH,
        r, g, b);
}

// kreira kompletan prikaz HH:MM:SS u centru ekrana
static void drawTimeDisplay()
{
    // razdvojimo cifre
    int hT = g_hours / 10;
    int hU = g_hours % 10;
    int mT = g_minutes / 10;
    int mU = g_minutes % 10;
    int sT = g_seconds / 10;
    int sU = g_seconds % 10;

    // pozicioniranje (NDC koordinate)
    float centerY = 0.0f;
    float digitW = 0.12f;
    float digitH = 0.25f;
    float spacing = 0.18f;   // razmak između centara cifara
    float colonGap = 0.10f;   // dodatni razmak oko ':' 

    // ručno odredimo x koordinate za 6 cifara + 2 dvotačke
    //   H T   H U   :   M T   M U   :   S T   S U
    float startX = -0.65f;

    float x_hT = startX;
    float x_hU = x_hT + spacing;

    float x_colon1 = x_hU + colonGap;

    float x_mT = x_colon1 + colonGap;
    float x_mU = x_mT + spacing;

    float x_colon2 = x_mU + colonGap;

    float x_sT = x_colon2 + colonGap;
    float x_sU = x_sT + spacing;

    float r = 1.0f, g = 1.0f, b = 1.0f; // bela boja

    // crtanje cifara
    drawDigit(hT, x_hT, centerY, digitW, digitH, r, g, b);
    drawDigit(hU, x_hU, centerY, digitW, digitH, r, g, b);

    drawColon(x_colon1, centerY, digitH, r, g, b);

    drawDigit(mT, x_mT, centerY, digitW, digitH, r, g, b);
    drawDigit(mU, x_mU, centerY, digitW, digitH, r, g, b);

    drawColon(x_colon2, centerY, digitH, r, g, b);

    drawDigit(sT, x_sT, centerY, digitW, digitH, r, g, b);
    drawDigit(sU, x_sU, centerY, digitW, digitH, r, g, b);
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

    // inicijalizacija sata
    initClock();
}


// === updateAndRender: jedan frame (input + logika + crtanje) ===
void updateAndRender(GLFWwindow* window) {
    // --- 1) input ---
    glfwPollEvents();

    if (currentScreen == Screen::TIME) {
        updateClock();
    }

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

    if (currentScreen == Screen::TIME) {
        // prvo nacrtaj vreme
        drawTimeDisplay();

        // pa onda strelicu desno
        drawQuad(arrowRightTime.xMin, arrowRightTime.xMax,
            arrowRightTime.yMin, arrowRightTime.yMax,
            1.0f, 1.0f, 1.0f);
    }

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

void initClock() {
    // Uzimamo trenutno lokalno vreme sa sistema
    std::time_t now = std::time(nullptr);
    std::tm lt{};
    localtime_s(&lt, &now);

    g_hours = lt.tm_hour;
    g_minutes = lt.tm_min;
    g_seconds = lt.tm_sec;


    lastClockUpdate = glfwGetTime();
}

void updateClock() {
    double current = glfwGetTime();
    double diff = current - lastClockUpdate;

    // povećavamo vreme svakih 1s (ako kasni frame, nadoknadimo više sekundi)
    if (diff >= 1.0) {
        int steps = static_cast<int>(diff);   // koliko sekundi je prošlo
        lastClockUpdate += steps;

        while (steps-- > 0) {
            g_seconds++;

            if (g_seconds >= 60) {
                g_seconds = 0;
                g_minutes++;
            }
            if (g_minutes >= 60) {
                g_minutes = 0;
                g_hours++;
            }
            if (g_hours >= 24) {
                g_hours = 0;
            }
        }

        // (za debug): ispiši u konzolu
        std::cout << "Time: "
            << (g_hours < 10 ? "0" : "") << g_hours << ":"
            << (g_minutes < 10 ? "0" : "") << g_minutes << ":"
            << (g_seconds < 10 ? "0" : "") << g_seconds << "\n";
    }
}

