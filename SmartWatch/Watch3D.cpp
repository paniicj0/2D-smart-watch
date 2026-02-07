#include "Watch3D.h"
#include "App.h"          // render2DFrame + Screen + buttons + currentScreen
#include "Math3D.h"

#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>

#include "stb_image.h"

// =====================
//  Settings
// =====================
static const int   SCREEN_TEX_W = 1024;
static const int   SCREEN_TEX_H = 1024;

enum Mode { MODE_WORLD, MODE_WATCH };
static Mode g_mode = MODE_WORLD;

// camera
static Vec3  g_camPos{ 0.0f, 0.7f, 2.2f };
static float g_camPitch = -0.2f;   // rad
static float g_camYaw = -1.570796f; // gleda ka -Z
static bool  g_firstMouse = true;
static double g_lastMx = 0.0, g_lastMy = 0.0;
static float g_baseCamY = 0.7f;

// watch move
static float g_watchT = 0.0f;   // 0 desno, 1 ispred
static bool  g_moveFront = false;
static bool  g_spaceDown = false;

// running sim (samo HEART screen)
static bool  g_running = false;
static float g_runTime = 0.0f;

// lighting
static Vec3 g_lightDir{ 0.15f, -1.0f, 0.10f };
static Vec3 g_lightColor{ 1.0f, 1.0f, 1.0f };

// =====================
//  GL objects
// =====================
static GLuint g_meshProg = 0;

static GLuint g_cubeVAO = 0, g_cubeVBO = 0;
static GLuint g_planeVAO = 0, g_planeVBO = 0;

static GLuint g_groundTex = 0;

static GLuint g_screenFBO = 0;
static GLuint g_screenTex = 0;
static GLuint g_screenDepth = 0;

// ground segments
struct GroundSeg { float z; };
static std::vector<GroundSeg> g_ground;
static const float GROUND_L = 5.0f;
static const int   GROUND_N = 6;

// 3D click debouncing (odvojeno od 2D)
static bool leftMouseDownLastFrame3D = false;

static bool g_enableDepth = true;
static bool g_enableCull = true;


// =====================
//  Helpers
// =====================
static GLuint compileFromFile(GLenum type, const char* path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Ne mogu da otvorim sejder: " << path << "\n";
        return 0;
    }
    std::stringstream ss;
    ss << file.rdbuf();
    std::string src = ss.str();
    const char* c = src.c_str();

    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &c, nullptr);
    glCompileShader(sh);

    GLint ok = 0; glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048]; glGetShaderInfoLog(sh, 2048, nullptr, log);
        std::cerr << "Greska u sejderu " << path << ":\n" << log << "\n";
    }
    return sh;
}

static GLuint linkProgram(const char* vsPath, const char* fsPath) {
    GLuint vs = compileFromFile(GL_VERTEX_SHADER, vsPath);
    GLuint fs = compileFromFile(GL_FRAGMENT_SHADER, fsPath);

    GLuint p = glCreateProgram();
    if (vs) glAttachShader(p, vs);
    if (fs) glAttachShader(p, fs);

    glLinkProgram(p);

    GLint ok = 0; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048]; glGetProgramInfoLog(p, 2048, nullptr, log);
        std::cerr << "Link greska:\n" << log << "\n";
    }

    if (vs) glDeleteShader(vs);
    if (fs) glDeleteShader(fs);

    return p;
}

static GLuint makeSolidTexture(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255) {
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    unsigned char px[4] = { r,g,b,a };
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    return tex;
}

static void makeCube() {
    // pos(3), normal(3), uv(2)
    const float v[] = {
        // +Z
        -0.5f,-0.5f, 0.5f,  0,0,1,  0,0,
         0.5f,-0.5f, 0.5f,  0,0,1,  1,0,
         0.5f, 0.5f, 0.5f,  0,0,1,  1,1,
        -0.5f,-0.5f, 0.5f,  0,0,1,  0,0,
         0.5f, 0.5f, 0.5f,  0,0,1,  1,1,
        -0.5f, 0.5f, 0.5f,  0,0,1,  0,1,
        // -Z
        -0.5f,-0.5f,-0.5f,  0,0,-1, 0,0,
         0.5f, 0.5f,-0.5f,  0,0,-1, 1,1,
         0.5f,-0.5f,-0.5f,  0,0,-1, 1,0,
        -0.5f,-0.5f,-0.5f,  0,0,-1, 0,0,
        -0.5f, 0.5f,-0.5f,  0,0,-1, 0,1,
         0.5f, 0.5f,-0.5f,  0,0,-1, 1,1,
         // +X
          0.5f,-0.5f,-0.5f,  1,0,0,  0,0,
          0.5f, 0.5f, 0.5f,  1,0,0,  1,1,
          0.5f,-0.5f, 0.5f,  1,0,0,  1,0,
          0.5f,-0.5f,-0.5f,  1,0,0,  0,0,
          0.5f, 0.5f,-0.5f,  1,0,0,  0,1,
          0.5f, 0.5f, 0.5f,  1,0,0,  1,1,
          // -X
          -0.5f,-0.5f,-0.5f, -1,0,0,  0,0,
          -0.5f,-0.5f, 0.5f, -1,0,0,  1,0,
          -0.5f, 0.5f, 0.5f, -1,0,0,  1,1,
          -0.5f,-0.5f,-0.5f, -1,0,0,  0,0,
          -0.5f, 0.5f, 0.5f, -1,0,0,  1,1,
          -0.5f, 0.5f,-0.5f, -1,0,0,  0,1,
          // +Y
          -0.5f, 0.5f,-0.5f,  0,1,0,  0,0,
          -0.5f, 0.5f, 0.5f,  0,1,0,  0,1,
           0.5f, 0.5f, 0.5f,  0,1,0,  1,1,
          -0.5f, 0.5f,-0.5f,  0,1,0,  0,0,
           0.5f, 0.5f, 0.5f,  0,1,0,  1,1,
           0.5f, 0.5f,-0.5f,  0,1,0,  1,0,
           // -Y
           -0.5f,-0.5f,-0.5f,  0,-1,0, 0,0,
            0.5f,-0.5f, 0.5f,  0,-1,0, 1,1,
           -0.5f,-0.5f, 0.5f,  0,-1,0, 0,1,
           -0.5f,-0.5f,-0.5f,  0,-1,0, 0,0,
            0.5f,-0.5f,-0.5f,  0,-1,0, 1,0,
            0.5f,-0.5f, 0.5f,  0,-1,0, 1,1,
    };

    glGenVertexArrays(1, &g_cubeVAO);
    glGenBuffers(1, &g_cubeVBO);
    glBindVertexArray(g_cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, g_cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));

    glBindVertexArray(0);
}

static void makePlane() {
    // unit plane on XZ (y=0), CCW winding when viewed from +Y
    const float v[] = {
        // tri 1
        -0.5f, 0.0f, -0.5f,   0,1,0,   0,0,
         0.5f, 0.0f,  0.5f,   0,1,0,   1,1,
         0.5f, 0.0f, -0.5f,   0,1,0,   1,0,

         // tri 2
         -0.5f, 0.0f, -0.5f,   0,1,0,   0,0,
         -0.5f, 0.0f,  0.5f,   0,1,0,   0,1,
          0.5f, 0.0f,  0.5f,   0,1,0,   1,1,
    };

    glGenVertexArrays(1, &g_planeVAO);
    glGenBuffers(1, &g_planeVBO);

    glBindVertexArray(g_planeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, g_planeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));

    glBindVertexArray(0);
}


static void makeScreenFBO() {
    glGenFramebuffers(1, &g_screenFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, g_screenFBO);

    glGenTextures(1, &g_screenTex);
    glBindTexture(GL_TEXTURE_2D, g_screenTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, SCREEN_TEX_W, SCREEN_TEX_H, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_screenTex, 0);

    glGenRenderbuffers(1, &g_screenDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, g_screenDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, SCREEN_TEX_W, SCREEN_TEX_H);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, g_screenDepth);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "FBO nije kompletan!\n";
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// 2D point in triangle + barycentric
static bool barycentric(const Vec2& p, const Vec2& a, const Vec2& b, const Vec2& c, float& u, float& v, float& w) {
    float den = (b.y - c.y) * (a.x - c.x) + (c.x - b.x) * (a.y - c.y);
    if (std::fabs(den) < 1e-6f) return false;
    u = ((b.y - c.y) * (p.x - c.x) + (c.x - b.x) * (p.y - c.y)) / den;
    v = ((c.y - a.y) * (p.x - c.x) + (a.x - c.x) * (p.y - c.y)) / den;
    w = 1.0f - u - v;
    return (u >= 0 && v >= 0 && w >= 0);
}

// mouse -> UV on projected quad for XZ plane
static bool mouseToScreenUV_XZ(GLFWwindow* window, const Mat4& MVP, int winW, int winH, float& outU, float& outV) {
    // corners for XZ plane (y=0)
    Vec4 corners[4] = {
        {-0.5f, 0.0f,-0.5f, 1.0f},
        { 0.5f, 0.0f,-0.5f, 1.0f},
        { 0.5f, 0.0f, 0.5f, 1.0f},
        {-0.5f, 0.0f, 0.5f, 1.0f}
    };

    Vec2 sp[4];
    for (int i = 0; i < 4; ++i) {
        Vec4 clip = mul(MVP, corners[i]);
        if (std::fabs(clip.w) < 1e-6f) return false;

        float ndcX = clip.x / clip.w;
        float ndcY = clip.y / clip.w;
        sp[i].x = (ndcX * 0.5f + 0.5f) * winW;
        sp[i].y = (1.0f - (ndcY * 0.5f + 0.5f)) * winH;
    }

    double mx, my; glfwGetCursorPos(window, &mx, &my);
    Vec2 p{ (float)mx, (float)my };

    Vec2 uv[4] = { {0,0},{1,0},{1,1},{0,1} };

    float a, b, c;
    if (barycentric(p, sp[0], sp[1], sp[2], a, b, c)) {
        outU = a * uv[0].x + b * uv[1].x + c * uv[2].x;
        outV = a * uv[0].y + b * uv[1].y + c * uv[2].y;
        return true;
    }
    if (barycentric(p, sp[0], sp[2], sp[3], a, b, c)) {
        outU = a * uv[0].x + b * uv[2].x + c * uv[3].x;
        outV = a * uv[0].y + b * uv[2].y + c * uv[3].y;
        return true;
    }
    return false;
}

static bool justLeftClick(GLFWwindow* window) {
    int st = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
    bool just = (st == GLFW_PRESS && !leftMouseDownLastFrame3D);
    leftMouseDownLastFrame3D = (st == GLFW_PRESS);
    return just;
}

static void setMode(GLFWwindow* window, Mode m) {
    g_mode = m;
    if (g_mode == MODE_WATCH) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        g_firstMouse = true;
    }
    else {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        g_firstMouse = true;
    }
}

static GLuint loadTexture2D(const char* path, bool srgb = false) {
    int w, h, n;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(path, &w, &h, &n, 0);
    if (!data) {
        std::cerr << "Ne mogu da ucitam teksturu: " << path << "\n";
        return 0;
    }

    GLenum format = (n == 4) ? GL_RGBA : (n == 3) ? GL_RGB : GL_RED;
    GLenum internal = format;
    if (srgb && format == GL_RGB) internal = GL_SRGB8;
    if (srgb && format == GL_RGBA) internal = GL_SRGB8_ALPHA8;

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexImage2D(GL_TEXTURE_2D, 0, internal, w, h, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    // Ponavljanje (tiling) — važno za “beskonačan” put
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    // Filtriranje
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);
    return tex;
}


// =====================
//  Public API
// =====================
void init3D(GLFWwindow* window, int winW, int winH) {
    (void)window; (void)winW; (void)winH;

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    g_meshProg = linkProgram("Shaders/mesh3d.vert", "Shaders/mesh3d.frag");

    makeCube();
    makePlane();
    makeScreenFBO();

    // ground texture (privremeno)
    g_groundTex = loadTexture2D("Resource Files/road.png", false);
    if (!g_groundTex) {
        // fallback ako slika ne postoji
        g_groundTex = makeSolidTexture(110, 110, 110);
    }


    // segments
    g_ground.clear();
    for (int i = 0; i < GROUND_N; ++i) g_ground.push_back({ -i * GROUND_L });

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

void cleanup3D() {
    if (g_meshProg) glDeleteProgram(g_meshProg);
    if (g_cubeVAO) glDeleteVertexArrays(1, &g_cubeVAO);
    if (g_cubeVBO) glDeleteBuffers(1, &g_cubeVBO);
    if (g_planeVAO) glDeleteVertexArrays(1, &g_planeVAO);
    if (g_planeVBO) glDeleteBuffers(1, &g_planeVBO);
    if (g_groundTex) glDeleteTextures(1, &g_groundTex);
    if (g_screenTex) glDeleteTextures(1, &g_screenTex);
    if (g_screenDepth) glDeleteRenderbuffers(1, &g_screenDepth);
    if (g_screenFBO) glDeleteFramebuffers(1, &g_screenFBO);
}

void updateAndRender3D(GLFWwindow* window) {
    // dt
    static double last = glfwGetTime();
    double now = glfwGetTime();
    float dt = (float)(now - last);
    last = now;

    // window size
    int winW, winH;
    glfwGetFramebufferSize(window, &winW, &winH);
    float aspect = winH > 0 ? (float)winW / (float)winH : 1.0f;

    // SPACE toggle
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && !g_spaceDown) {
        g_spaceDown = true;
        if (g_mode == MODE_WORLD) {
            setMode(window, MODE_WATCH);
            g_moveFront = true;
        }
        else {
            setMode(window, MODE_WORLD);
            g_moveFront = false;
        }
    }
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_RELEASE) g_spaceDown = false;

    // mouse pitch (world mode)
    if (g_mode == MODE_WORLD) {
        double mx, my; glfwGetCursorPos(window, &mx, &my);
        if (g_firstMouse) { g_lastMx = mx; g_lastMy = my; g_firstMouse = false; }
        double dy = my - g_lastMy;
        g_lastMx = mx; g_lastMy = my;

        g_camPitch += (float)dy * 0.0025f;
        g_camPitch = clampf(g_camPitch, -1.1f, 0.4f);
    }

    // running allowed only on HEART screen
    g_running = (currentScreen == Screen::HEART);
    if (g_running) {
        g_runTime += dt;

        // camera bob (samo Y)
        float bob = std::sin(g_runTime * 6.5f) * 0.03f;
        g_camPos.y = g_baseCamY + bob;

        // ---------- INFINITE GROUND ----------
        const float speed = 2.2f;

        // 1) pomeri sve segmente ka kameri (ka +Z)
        for (auto& s : g_ground) {
            s.z += speed * dt;
        }

        // 2) prag kad segment "prođe" kameru (tj. izađe iz kadra ka nama)
        // kamera ti je na g_camPos.z = 2.2, pa prag treba da bude vezan za kameru
        const float frontLimit = g_camPos.z + (GROUND_L * 0.5f);

        // 3) reset: sve što pređe frontLimit ide nazad iza najudaljenijeg segmenta
        // (najudaljeniji "iza" je najmanji z)
        while (true) {
            int idx = -1;

            // nađi segment koji je prešao prag
            for (int i = 0; i < (int)g_ground.size(); ++i) {
                if (g_ground[i].z > frontLimit) { idx = i; break; }
            }
            if (idx == -1) break; // nema više za reciklažu

            // nađi trenutno najudaljeniji iza (najmanji z)
            float minZ = 1e9f;
            for (auto& s : g_ground) minZ = std::min(minZ, s.z);

            // prebaci ga iza svih (još jednu dužinu segmenta)
            g_ground[idx].z = minZ - GROUND_L;
        }

    }
    else {
        g_runTime = 0.0f;
        g_camPos.y = g_baseCamY;
    }


    // smooth watch pose
    float target = g_moveFront ? 1.0f : 0.0f;
    float k = 1.0f - std::exp(-6.0f * dt);
    g_watchT = lerpf(g_watchT, target, k);

    // ========= 1) Render 2D watch screen to texture =========
    glBindFramebuffer(GL_FRAMEBUFFER, g_screenFBO);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    render2DFrame(window, SCREEN_TEX_W, SCREEN_TEX_H, false);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // ========= 2) Setup camera matrices =========
    Vec3 forward{
        std::cos(g_camYaw) * std::cos(g_camPitch),
        std::sin(g_camPitch),
        std::sin(g_camYaw) * std::cos(g_camPitch)
    };
    forward = normalize(forward);
    Vec3 targetPos = g_camPos + forward;

    Mat4 V = lookAt(g_camPos, targetPos, { 0,1,0 });
    Mat4 P = perspective(60.0f * 3.14159f / 180.0f, aspect, 0.05f, 50.0f);

    // ========= 3) Scene transforms (hand+watch) =========
    Vec3 camRight = normalize(cross(forward, { 0,1,0 }));
    Vec3 camUp = cross(camRight, forward);

    Vec3 handSideWorld = g_camPos + camRight * 0.65f + camUp * (-0.15f) + forward * 0.55f;
    Vec3 handFrontWorld = g_camPos + camRight * 0.00f + camUp * (-0.10f) + forward * 0.55f;
    Vec3 handPos = lerp(handSideWorld, handFrontWorld, g_watchT);

    Mat4 Mhand = mul(
        translate(handPos),
        mul(rotateY(g_camYaw + 3.14159f), rotateX(-0.35f))
    );

    // watch body
    const float watchW = 0.22f;
    const float watchH = 0.12f;
    const float watchD = 0.26f;

    Mat4 Mwatch = mul(
        Mhand,
        mul(translate({ 0.0f, 0.02f, 0.0f }),
            scale({ watchW, watchH, watchD }))
    );

    // ===== Screen glued on top (XZ plane) =====
    const float eps = 0.0015f; // protiv treperenja
    Mat4 Mscreen = mul(
        Mhand,
        mul(
            translate({ 0.0f, 0.02f + watchH * 0.5f + eps, 0.0f }), // na vrh kućišta
            mul(
                rotateZ(3.14159f), // ako je naopako, probaj rotateZ(pi) umesto ovoga
                scale({ watchW * 0.92f, 1.0f, watchD * 0.92f })
            )
        )
    );

    // screen light pos (center)
    Vec4 sp = mul(Mhand, Vec4{ 0.0f, 0.03f, 0.14f, 1.0f });
    Vec3 screenLightPos{ sp.x, sp.y, sp.z };

    // ========= 4) Click on arrows (MODE_WATCH only) =========
    if (g_mode == MODE_WATCH && justLeftClick(window)) {
        Mat4 MVP = mul(P, mul(V, Mscreen));
        float u, v;
        if (mouseToScreenUV_XZ(window, MVP, winW, winH, u, v)) {

            // jedan flip po V (zbog 2D koordinata)
            v = 1.0f - v;

            auto ndcToUV = [](float x, float y) {
                return Vec2{ (x + 1.0f) * 0.5f, (y + 1.0f) * 0.5f };
                };

            auto insideUV = [&](const Button& b) {
                Vec2 mn = ndcToUV(b.xMin, b.yMin);
                Vec2 mx = ndcToUV(b.xMax, b.yMax);
                return u >= mn.x && u <= mx.x && v >= mn.y && v <= mx.y;
                };

            switch (currentScreen) {
            case Screen::TIME:
                if (insideUV(arrowRightTime)) currentScreen = Screen::HEART;
                break;
            case Screen::HEART:
                if (insideUV(arrowLeftHeart)) currentScreen = Screen::TIME;
                else if (insideUV(arrowRightHeart)) currentScreen = Screen::BATTERY;
                break;
            case Screen::BATTERY:
                if (insideUV(arrowLeftBattery)) currentScreen = Screen::HEART;
                break;
            }
        }
    }

    // ========= 5) Render 3D world =========
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    if (g_enableDepth) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (g_enableCull)  glEnable(GL_CULL_FACE);  else glDisable(GL_CULL_FACE);

    static bool f1Down = false, f2Down = false;

    if (glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS && !f1Down) { f1Down = true; g_enableDepth = !g_enableDepth; }
    if (glfwGetKey(window, GLFW_KEY_F1) == GLFW_RELEASE) f1Down = false;

    if (glfwGetKey(window, GLFW_KEY_F2) == GLFW_PRESS && !f2Down) { f2Down = true; g_enableCull = !g_enableCull; }
    if (glfwGetKey(window, GLFW_KEY_F2) == GLFW_RELEASE) f2Down = false;


    glViewport(0, 0, winW, winH);
    glClearColor(0.05f, 0.06f, 0.08f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(g_meshProg);

    auto setMat4 = [&](const char* name, const Mat4& M) {
        glUniformMatrix4fv(glGetUniformLocation(g_meshProg, name), 1, GL_FALSE, M.m);
        };
    auto setVec3 = [&](const char* name, const Vec3& v3) {
        glUniform3f(glGetUniformLocation(g_meshProg, name), v3.x, v3.y, v3.z);
        };

    setMat4("uView", V);
    setMat4("uProj", P);
    setVec3("uLightDir", normalize(g_lightDir));
    setVec3("uLightColor", g_lightColor);
    setVec3("uViewPos", g_camPos);

    setVec3("uScreenLightPos", screenLightPos);
    setVec3("uScreenLightColor", { 0.35f, 0.55f, 1.0f });
    glUniform1f(glGetUniformLocation(g_meshProg, "uScreenLightRadius"), 1.2f);

    // ---------- ground ----------
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_groundTex);
    glUniform1i(glGetUniformLocation(g_meshProg, "uTex0"), 0);
    glUniform1i(glGetUniformLocation(g_meshProg, "uUseTexture"), 1);
    glUniform3f(glGetUniformLocation(g_meshProg, "uBaseColor"), 0.65f, 0.65f, 0.65f);
    glUniform1f(glGetUniformLocation(g_meshProg, "uEmissive"), 0.0f);

    glBindVertexArray(g_planeVAO);
    for (auto& s : g_ground) {
        const float ROAD_W = 3.2f;  // NOVO
        Mat4 M = mul(translate({ 0.0f, 0.0f, s.z }), scale({ ROAD_W, 1.0f, GROUND_L }));

        setMat4("uModel", M);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    // ---------- buildings ----------
    glBindVertexArray(g_cubeVAO);
    glUniform1i(glGetUniformLocation(g_meshProg, "uUseTexture"), 0);
    glUniform1f(glGetUniformLocation(g_meshProg, "uEmissive"), 0.0f);

    const float spacing = 4.0f;
    const float baseZ = -6.0f;
    const float speedB = 2.2f;

    float runOffset = g_running ? (float)std::fmod(g_runTime * speedB, spacing) : 0.0f;

    // bliže putu (put je širine ~6 => ivica oko x=±3.0)
    const float ROAD_W = 3.2f;
    const float SIDE_OFFSET = 0.55f;
    const float xLeft = -(ROAD_W * 0.5f + SIDE_OFFSET);
    const float xRight = (ROAD_W * 0.5f + SIDE_OFFSET);


    for (int i = 0; i < 14; ++i) {
        float z = baseZ - i * spacing + runOffset;

        // LEVO
        {
            Mat4 Ml = mul(translate({ xLeft, 0.7f, z }),
                scale({ 0.9f, 1.6f, 0.9f }));
            setMat4("uModel", Ml);

            // svetlije da se vidi
            glUniform3f(glGetUniformLocation(g_meshProg, "uBaseColor"),
                0.45f, 0.46f, 0.52f);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        // DESNO
        {
            Mat4 Mr = mul(translate({ xRight, 0.65f, z - 1.7f }),
                scale({ 0.85f, 1.4f, 0.85f }));
            setMat4("uModel", Mr);

            glUniform3f(glGetUniformLocation(g_meshProg, "uBaseColor"),
                0.40f, 0.48f, 0.40f);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }
    }


    // ---------- hand ----------
    Mat4 MhandMesh = mul(Mhand, scale({ 0.16f, 0.08f, 0.55f }));
    setMat4("uModel", MhandMesh);
    glUniform3f(glGetUniformLocation(g_meshProg, "uBaseColor"), 0.72f, 0.56f, 0.45f);
    glUniform1f(glGetUniformLocation(g_meshProg, "uEmissive"), 0.0f);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    // ---------- watch body ----------
    setMat4("uModel", Mwatch);
    glUniform3f(glGetUniformLocation(g_meshProg, "uBaseColor"), 0.10f, 0.10f, 0.12f);
    glUniform1f(glGetUniformLocation(g_meshProg, "uEmissive"), 0.0f);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    // ---------- watch screen (textured + emissive) ----------
    glBindVertexArray(g_planeVAO);
    setMat4("uModel", Mscreen);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_screenTex);
    glUniform1i(glGetUniformLocation(g_meshProg, "uTex0"), 0);
    glUniform1i(glGetUniformLocation(g_meshProg, "uUseTexture"), 1);
    glUniform3f(glGetUniformLocation(g_meshProg, "uBaseColor"), 1.0f, 1.0f, 1.0f);
    glUniform1f(glGetUniformLocation(g_meshProg, "uEmissive"), 0.35f);

    glDisable(GL_CULL_FACE);

    // anti z-fighting:
    glDepthMask(GL_FALSE);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDepthMask(GL_TRUE);

    glEnable(GL_CULL_FACE);

    glBindVertexArray(0);
    // ---- signature overlay on big screen ----
    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    renderSignatureOverlay3D(w, h);

}
