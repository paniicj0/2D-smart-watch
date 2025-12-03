#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "App.h"
#include <ctime>   // za lokalno vreme
#include <cstdlib>   // za rand
#include <cmath>     

// === Definicije globalnih promenljivih ===
Screen currentScreen = Screen::TIME;
bool leftMouseDownLastFrame = false;
static GLFWcursor* g_heartCursor = nullptr;
GLuint arrowLeftTexture = 0;
GLuint arrowRightTexture = 0;

GLuint signatureTexture = 0;
GLuint signatureVAO = 0;
GLuint signatureVBO = 0;

GLuint warningTexture = 0;

GLuint quadVAO = 0;
GLuint quadVBO = 0;
GLuint shaderProgram = 0;

// === Sat ===
int g_hours = 0;
int g_minutes = 0;
int g_seconds = 0;

static double lastClockUpdate = 0.0;

// === Heart / BPM ===
float g_bpm = 70.0f;
float g_bpmTarget = 70.0f;

static float g_restBpmMin = 60.0f;
static float g_restBpmMax = 80.0f;
static float g_maxBpm = 210.0f;

// vreme za "lagano" menjanje random BPM-a u mirovanju
static double lastHeartRandomChange = 0.0;

// za EKG animaciju (za sada samo čuvamo, kasnije koristimo za teksture)
static float g_ekgScaleX = 1.0f;   // koliko je talas "zgusnut"
static float g_ekgSpeed = 0.5f;  // brzina pomeranja
static double lastHeartUpdateTime = 0.0;

// === EKG tekstura & shader ===
GLuint ekgTexture = 0;
GLuint ekgVAO = 0;
GLuint ekgVBO = 0;
GLuint ekgShaderProgram = 0;

float g_ekgScroll = 0.0f;   // koliko smo "odskrolovali" teksturu ulevo

// === Baterija ===
int g_batteryPercent = 100;          // od 0 do 100
static double lastBatteryUpdateTime = 0.0;

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

// === GLSL šejderi za teksturisani EKG quad ===
const char* ekgVertexShaderSrc = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTex;

out vec2 TexCoord;

uniform float uScaleX;   // koliko "zgusnemo" talas
uniform float uOffsetX;  // pomeraj ulevo/desno

void main() {
    TexCoord = vec2(aTex.x * uScaleX + uOffsetX, aTex.y);
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)";

const char* ekgFragmentShaderSrc = R"(
#version 330 core
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D uTexture;
uniform float uGlobalAlpha;   // dodato

void main() {
    vec4 tex = texture(uTexture, TexCoord);
    // množimo originalnu alfu sa globalnom
    FragColor = vec4(tex.rgb, tex.a * uGlobalAlpha);
}
)";




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


// učitavanje teksture iz fajla
static GLuint loadTexture(const char* path) {
    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(1); // ako ti tekstura ispadne naopako, ovo pomaže
    unsigned char* data = stbi_load(path, &width, &height, &nrChannels, 4); // forsiramo RGBA

    if (!data) {
        std::cerr << "Failed to load texture: " << path << std::endl;
        return 0;
    }

    std::cout << "Loaded texture: " << path
        << " (" << width << "x" << height << ")\n";


    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
        width, height, 0,
        GL_RGBA, GL_UNSIGNED_BYTE, data);

    glGenerateMipmap(GL_TEXTURE_2D);

    // osnovni parametri za ponavljanje i filtriranje
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); // ponavljanje po X
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);
    return texID;
}

static void initEKG() {
    // shader
    GLuint vs = compileShader(GL_VERTEX_SHADER, ekgVertexShaderSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, ekgFragmentShaderSrc);

    ekgShaderProgram = glCreateProgram();
    glAttachShader(ekgShaderProgram, vs);
    glAttachShader(ekgShaderProgram, fs);
    glLinkProgram(ekgShaderProgram);

    glDeleteShader(vs);
    glDeleteShader(fs);

    // quad sa pozicijom + UV
    glGenVertexArrays(1, &ekgVAO);
    glGenBuffers(1, &ekgVBO);

    glBindVertexArray(ekgVAO);
    glBindBuffer(GL_ARRAY_BUFFER, ekgVBO);

    // 6 verteksa, svaki ima (x, y, u, v)
    glBufferData(GL_ARRAY_BUFFER, 6 * 4 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glBindVertexArray(0);

    // tekstura
    ekgTexture = loadTexture("Resource Files/ekg.jpg");
    if (ekgTexture == 0) {
        std::cerr << "WARNING: ekgTexture not loaded!\n";
    }

    // 🆕 warning tekstura
    warningTexture = loadTexture("Resource Files/zio.png");
    if (warningTexture == 0) {
        std::cerr << "WARNING: warningTexture not loaded!\n";
    }

    glUseProgram(ekgShaderProgram);
    GLint texLoc = glGetUniformLocation(ekgShaderProgram, "uTexture");
    glUniform1i(texLoc, 0); // texture unit 0
    glUseProgram(0);
}

void initSignature() {
    glGenVertexArrays(1, &signatureVAO);
    glGenBuffers(1, &signatureVBO);

    glBindVertexArray(signatureVAO);
    glBindBuffer(GL_ARRAY_BUFFER, signatureVBO);

    // 6 verteksa (position + UV)
    glBufferData(GL_ARRAY_BUFFER, 6 * 4 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glBindVertexArray(0);

    // učitavamo PNG
    signatureTexture = loadTexture("Resource Files/potpis.png");

    if (!signatureTexture) {
        std::cout << "Failed to load signature texture!\n";
    }
}

void drawSignature(float xMin, float xMax, float yMin, float yMax) {
    if (signatureTexture == 0) return;

    float vertices[24] = {
        xMin, yMin,  0.0f, 0.0f,
        xMax, yMin,  1.0f, 0.0f,
        xMax, yMax,  1.0f, 1.0f,

        xMin, yMin,  0.0f, 0.0f,
        xMax, yMax,  1.0f, 1.0f,
        xMin, yMax,  0.0f, 1.0f
    };

    glUseProgram(ekgShaderProgram);  // koristimo teksturisani shader

    GLint scaleLoc = glGetUniformLocation(ekgShaderProgram, "uScaleX");
    GLint offsetLoc = glGetUniformLocation(ekgShaderProgram, "uOffsetX");
    glUniform1f(scaleLoc, 1.0f);
    glUniform1f(offsetLoc, 0.0f);

    // ovde smanjujemo providnost – npr. 0.4f = 40% vidljivo
    GLint alphaLoc = glGetUniformLocation(ekgShaderProgram, "uGlobalAlpha");
    glUniform1f(alphaLoc, 0.4f);


    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, signatureTexture);

    glBindVertexArray(signatureVAO);
    glBindBuffer(GL_ARRAY_BUFFER, signatureVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    glBindVertexArray(0);
}



void initArrows()
{
    arrowLeftTexture = loadTexture("Resource Files/left-arrow.png");
    arrowRightTexture = loadTexture("Resource Files/right-arrow.png");

    if (!arrowLeftTexture) {
        std::cerr << "Failed to load arrow_left texture!\n";
    }
    if (!arrowRightTexture) {
        std::cerr << "Failed to load arrow_right texture!\n";
    }
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
    float dotH = size * 0.1f;
    float dotW = size * 0.10f;
    float offsetY = size * 0.20f;

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
    float spacing = 0.20f;   // razmak između centara cifara
    float colonGap = 0.12f;   // dodatni razmak oko ':' 

    // ručno odredimo x koordinate za 6 cifara + 2 dvotačke
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

static void drawNumber(int value,
    float centerX, float centerY,
    float digitW, float digitH,
    float spacing,
    float r, float g, float b)
{
    if (value < 0) value = 0;
    if (value > 999) value = 999;

    int hundreds = value / 100;
    int tens = (value / 10) % 10;
    int ones = value % 10;

    if (value >= 100) {
        // 3 cifre: H T O
        float startX = centerX - spacing;         
        float xH = startX;
        float xT = startX + spacing;
        float xO = startX + 2.0f * spacing;

        drawDigit(hundreds, xH, centerY, digitW, digitH, r, g, b);
        drawDigit(tens, xT, centerY, digitW, digitH, r, g, b);
        drawDigit(ones, xO, centerY, digitW, digitH, r, g, b);
    }
    else {
        // 2 cifre: T O
        int t = value / 10;
        int o = value % 10;

        float startX = centerX - spacing * 0.5f;
        float xT = startX;
        float xO = startX + spacing;

        drawDigit(t, xT, centerY, digitW, digitH, r, g, b);
        drawDigit(o, xO, centerY, digitW, digitH, r, g, b);
    }
}



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

    initClock();
    initHeart();
    initEKG();
    initBattery();
    initArrows();
    initSignature();
}

static void drawTexturedQuad(GLuint texture,
    float xMin, float xMax,
    float yMin, float yMax)
{
    if (texture == 0) return;

    float vertices[6 * 4] = {
        // x,    y,     u,   v
        xMin, yMin,   0.0f, 0.0f,
        xMax, yMin,   1.0f, 0.0f,
        xMax, yMax,   1.0f, 1.0f,

        xMin, yMin,   0.0f, 0.0f,
        xMax, yMax,   1.0f, 1.0f,
        xMin, yMax,   0.0f, 1.0f
    };

    glUseProgram(ekgShaderProgram);

    // za strelice: bez skaliranja i bez pomeranja
    GLint scaleLoc = glGetUniformLocation(ekgShaderProgram, "uScaleX");
    GLint offsetLoc = glGetUniformLocation(ekgShaderProgram, "uOffsetX");
    glUniform1f(scaleLoc, 1.0f);
    glUniform1f(offsetLoc, 0.0f);

    GLint alphaLoc = glGetUniformLocation(ekgShaderProgram, "uGlobalAlpha");
    glUniform1f(alphaLoc, 1.0f);   // potpuno vidljivo

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);

    glBindVertexArray(ekgVAO);
    glBindBuffer(GL_ARRAY_BUFFER, ekgVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}


// === updateAndRender: jedan frame (input + logika + crtanje) ===
void updateAndRender(GLFWwindow* window) {
    // --- 1) input ---
    glfwPollEvents();
	updateClock();
    updateBattery();

    if (currentScreen == Screen::HEART) {
        updateHeart(window);
    }

    // ESC gasi aplikaciju
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }

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
    if (currentScreen == Screen::TIME) {
        glClearColor(0.1f, 0.1f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        drawSignature(0.55f, 0.95f, -0.95f, -0.80f);
        drawTimeDisplay();
        drawTexturedQuad(arrowRightTexture,
            arrowRightTime.xMin, arrowRightTime.xMax,
            arrowRightTime.yMin, arrowRightTime.yMax);

    }
    else if (currentScreen == Screen::HEART) {
        // HEART ekran crtamo posebnom funkcijom
        drawHeartScreen();

    }
    else if (currentScreen == Screen::BATTERY) {
        drawBatteryScreen();
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

void initHeart() {
    // inicijalni random BPM između 60 i 80
    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    float t = static_cast<float>(std::rand()) / RAND_MAX;
    g_bpm = g_bpmTarget = g_restBpmMin + t * (g_restBpmMax - g_restBpmMin);

    lastHeartRandomChange = glfwGetTime();
    lastHeartUpdateTime = glfwGetTime();
}

void initBattery() {
    g_batteryPercent = 100;
    lastBatteryUpdateTime = glfwGetTime();
}

void updateClock() {
    double current = glfwGetTime();
    double diff = current - lastClockUpdate;

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

void updateHeart(GLFWwindow* window) {
    double now = glfwGetTime();
    double dt = now - lastHeartUpdateTime;
    if (dt < 0.0) dt = 0.0;
    lastHeartUpdateTime = now;

    // da li se drži taster D?
    bool running = (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS);

    if (running) {
        // trčanje: povećavamo target BPM ka maxBpm
        g_bpmTarget += 40.0f * static_cast<float>(dt);  // ~40 BPM po sekundi
        if (g_bpmTarget > g_maxBpm) g_bpmTarget = g_maxBpm;
    }
    else {
        // mirovanje: lagano vraćanje na random bazu 60-80 BPM
        if (now - lastHeartRandomChange > 2.0) {
            lastHeartRandomChange = now;
            float t = static_cast<float>(std::rand()) / RAND_MAX;
            float restTarget = g_restBpmMin + t * (g_restBpmMax - g_restBpmMin);
            g_bpmTarget = restTarget;
        }
    }

    // glatko približavanje current BPM ka targetu
    float lerpFactor = static_cast<float>(dt) * 2.0f; // brzina prilagođavanja
    if (lerpFactor > 1.0f) lerpFactor = 1.0f;

    g_bpm = g_bpm + (g_bpmTarget - g_bpm) * lerpFactor;

    // EKG scale: što veći BPM, to više otkucaja na ekranu
    float minMap = 60.0f;
    float maxMap = 200.0f;
    float clamped = std::fmax(minMap, std::fmin(maxMap, g_bpm));
    float alpha = (clamped - minMap) / (maxMap - minMap);
    g_ekgScaleX = 1.0f + alpha * 2.0f;

    // za debug:
    std::cout << "BPM: " << g_bpm << " (target: " << g_bpmTarget << ")\n";

    // skrolovanje EKG talasa ulevo
    g_ekgScroll += g_ekgSpeed * static_cast<float>(dt);
    // da ne raste u beskonačnost, samo ga "zamotamo"
    if (g_ekgScroll < -1000.0f) {
        g_ekgScroll += 1000.0f;
    }

}

static void drawEKGQuad(float xMin, float xMax, float yMin, float yMax) {
    if (ekgTexture == 0) {
        return;
    }

    // pozicija u NDC
    float vertices[6 * 4] = {
        //  x,      y,      u, v
        xMin, yMin,  0.0f, 0.0f,
        xMax, yMin,  1.0f, 0.0f,
        xMax, yMax,  1.0f, 1.0f,

        xMin, yMin,  0.0f, 0.0f,
        xMax, yMax,  1.0f, 1.0f,
        xMin, yMax,  0.0f, 1.0f
    };

    glUseProgram(ekgShaderProgram);

    // scale i offset
    GLint scaleLoc = glGetUniformLocation(ekgShaderProgram, "uScaleX");
    GLint offsetLoc = glGetUniformLocation(ekgShaderProgram, "uOffsetX");
    glUniform1f(scaleLoc, g_ekgScaleX);
    glUniform1f(offsetLoc, g_ekgScroll);

    GLint alphaLoc = glGetUniformLocation(ekgShaderProgram, "uGlobalAlpha");
    glUniform1f(alphaLoc, 1.0f);   // potpuno vidljivo

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ekgTexture);

    glBindVertexArray(ekgVAO);
    glBindBuffer(GL_ARRAY_BUFFER, ekgVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}


void drawHeartScreen() {
    glClearColor(0.3f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // pravougaonik zona EKG grafika (tamno siva "kutija")
    float boxXmin = -0.6f;
    float boxXmax = 0.6f;
    float boxYmin = -0.3f;
    float boxYmax = 0.3f;

    drawQuad(boxXmin, boxXmax, boxYmin, boxYmax,
        0.1f, 0.1f, 0.1f);

    // BPM broj iznad kutije
    int bpmInt = static_cast<int>(std::round(g_bpm));

    float bpmCenterX = 0.0f;   // centrirano po X
    float bpmCenterY = 0.85f;  // pri vrhu ekrana
    float bpmDigitW = 0.08f;
    float bpmDigitH = 0.18f;
    float bpmSpacing = 0.11f;


    int value = bpmInt;
    int numDigits = (value >= 100) ? 3 : 2;

    float rightEdgeX = bpmCenterX + (numDigits == 3 ? bpmSpacing : bpmSpacing * 0.5f);


    drawNumber(bpmInt, bpmCenterX, bpmCenterY,
        bpmDigitW, bpmDigitH, bpmSpacing,
        1.0f, 1.0f, 1.0f);   // bela boja


    // "linija" EKG bazne linije (zelena horizontalna)
    float lineThickness = 0.02f;
    drawQuad(boxXmin, boxXmax,
        -lineThickness * 0.5f, lineThickness * 0.5f,
        0.0f, 0.8f, 0.0f);

    // mapiramo [60, 200] -> [0, 1] da bi se promene lepo videle
    float minVis = 60.0f;
    float maxVis = 200.0f;
    float clampedBpm = std::fmax(minVis, std::fmin(maxVis, g_bpm));
    float normBpm = (clampedBpm - minVis) / (maxVis - minVis); // 0..1

    float barWidth = -0.8f + normBpm * 1.6f; // od -0.8 do +0.8

    drawQuad(-0.8f, barWidth,
        0.6f, 0.7f,
        0.8f, 0.8f, 0.0f); // žućkasta bar za BPM

    // EKG tekstura koja se pomera ulevo i "zgusne" sa BPM
    drawEKGQuad(boxXmin, boxXmax, boxYmin, boxYmax);

    // strelice: levo (nazad na TIME) i desno (na BATTERY)
    drawTexturedQuad(arrowLeftTexture,
        arrowLeftHeart.xMin, arrowLeftHeart.xMax,
        arrowLeftHeart.yMin, arrowLeftHeart.yMax);

    drawTexturedQuad(arrowRightTexture,
        arrowRightHeart.xMin, arrowRightHeart.xMax,
        arrowRightHeart.yMin, arrowRightHeart.yMax);


    // ako BPM pređe 200 – crveno upozorenje preko ekrana
    if (g_bpm > 200.0f) {
        // crveni overlay
        drawQuad(-1.0f, 1.0f, -1.0f, 1.0f,
            0.8f, 0.0f, 0.0f);

        // ako postoji warning tekstura – nacrtaj je preko
        if (warningTexture != 0) {
            glUseProgram(ekgShaderProgram);

            // za warning nećemo scale/scroll, pa ih resetujemo
            GLint scaleLoc = glGetUniformLocation(ekgShaderProgram, "uScaleX");
            GLint offsetLoc = glGetUniformLocation(ekgShaderProgram, "uOffsetX");
            glUniform1f(scaleLoc, 1.0f);
            glUniform1f(offsetLoc, 0.0f);

            // pravougaonik u sredini ekrana
            float quadXmin = -1.0f;
            float quadXmax = 1.0f;
            float quadYmin = -1.0f;
            float quadYmax = 1.0f;

            float vertices[6 * 4] = {
                // x,       y,       u, v
                quadXmin, quadYmin, 0.0f, 0.0f,
                quadXmax, quadYmin, 1.0f, 0.0f,
                quadXmax, quadYmax, 1.0f, 1.0f,

                quadXmin, quadYmin, 0.0f, 0.0f,
                quadXmax, quadYmax, 1.0f, 1.0f,
                quadXmin, quadYmax, 0.0f, 1.0f
            };

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, warningTexture);

            glBindVertexArray(ekgVAO);
            glBindBuffer(GL_ARRAY_BUFFER, ekgVBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);

            glDrawArrays(GL_TRIANGLES, 0, 6);

            glBindVertexArray(0);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
    }
}

void updateBattery() {
    double now = glfwGetTime();
    double diff = now - lastBatteryUpdateTime;

    // na svakih 10 sekundi smanji procenat za 1
    if (diff >= 10.0) {
        int steps = static_cast<int>(diff / 10.0); // koliko "desetki" je prošlo
        lastBatteryUpdateTime += steps * 10.0;

        g_batteryPercent -= steps;
        if (g_batteryPercent < 0) g_batteryPercent = 0;
    }
}

void drawBatteryScreen() {
    // pozadina
    glClearColor(0.0f, 0.15f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // telo baterije (okvir)
    float bodyXmin = -0.3f;
    float bodyXmax = 0.3f;
    float bodyYmin = -0.2f;
    float bodyYmax = 0.2f;

    float capXmin = bodyXmax;
    float capXmax = bodyXmax + 0.05f;
    float capYmin = -0.05f;
    float capYmax = 0.05f;

    // okvir – crtamo kao pravougaonike za ivice
    float border = 0.01f;
    // gornja ivica
    drawQuad(bodyXmin, bodyXmax, bodyYmax - border, bodyYmax, 1.0f, 1.0f, 1.0f);
    // donja ivica
    drawQuad(bodyXmin, bodyXmax, bodyYmin, bodyYmin + border, 1.0f, 1.0f, 1.0f);
    // leva ivica
    drawQuad(bodyXmin, bodyXmin + border, bodyYmin, bodyYmax, 1.0f, 1.0f, 1.0f);
    // desna ivica
    drawQuad(bodyXmax - border, bodyXmax, bodyYmin, bodyYmax, 1.0f, 1.0f, 1.0f);

    // kapica
    drawQuad(capXmin, capXmax, capYmin, capYmax, 1.0f, 1.0f, 1.0f);

    float innerXmin = bodyXmin + border * 2.0f;
    float innerXmax = bodyXmax - border * 2.0f;
    float innerYmin = bodyYmin + border * 2.0f;
    float innerYmax = bodyYmax - border * 2.0f;

    // širina punjenja prema procentu; desna ivica uvek na innerXmax
    float percent = static_cast<float>(g_batteryPercent);
    if (percent < 0.0f) percent = 0.0f;
    if (percent > 100.0f) percent = 100.0f;

    float fullWidth = innerXmax - innerXmin;
    float fillWidth = fullWidth * (percent / 100.0f);
    float fillXmax = innerXmax;
    float fillXmin = innerXmax - fillWidth;

    float r, g, b;
    if (g_batteryPercent > 20) {
        r = 0.0f; g = 0.8f; b = 0.0f;      // zelena
    }
    else if (g_batteryPercent > 10) {
        r = 0.8f; g = 0.8f; b = 0.0f;      // žuta
    }
    else {
        r = 0.8f; g = 0.0f; b = 0.0f;      // crvena
    }

    if (fillWidth > 0.0f) {
        drawQuad(fillXmin, fillXmax, innerYmin, innerYmax, r, g, b);
    }

    int shownPercent = g_batteryPercent;
    if (shownPercent < 0) shownPercent = 0;
    if (shownPercent > 100) shownPercent = 100;

    float numCenterX = 0.0f;
    float numCenterY = 0.6f;
    float digitW = 0.08f;
    float digitH = 0.18f;
    float digitSpacing = 0.11f;

    drawNumber(shownPercent, numCenterX, numCenterY,
        digitW, digitH, digitSpacing,
        1.0f, 1.0f, 1.0f);

    drawTexturedQuad(arrowLeftTexture,
        arrowLeftBattery.xMin, arrowLeftBattery.xMax,
        arrowLeftBattery.yMin, arrowLeftBattery.yMax);

    drawSignature(0.55f, 0.95f, -0.95f, -0.80f);


}

void initHeartCursor(GLFWwindow* window)
{
    int width, height, channels;
    stbi_set_flip_vertically_on_load(0);
    unsigned char* data = stbi_load("Resource Files/love-pointer.png",
		&width, &height, &channels, 4);

    if (!data) {
        std::cerr << "Failed to load cursor texture!\n";
        return;
	}

    GLFWimage image;
    image.width = width;
    image.height = height;
    image.pixels = data;
    
	int hotspotX = width / 2;
	int hotspotY = height / 2;

	GLFWcursor* cursor = glfwCreateCursor(&image, hotspotX, hotspotY);
	stbi_image_free(data);

    if (!cursor) {
        std::cerr << "Failed to create cursor!\n";
        return;
	}

	g_heartCursor = cursor;

	glfwSetCursor(window, g_heartCursor);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

}

void destroyHeartCursor()
{
    if (g_heartCursor) {
        glfwDestroyCursor(g_heartCursor);
        g_heartCursor = nullptr;
    }
}
