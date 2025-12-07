#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>

// ekrani
enum class Screen {
    TIME,
    HEART,
    BATTERY
};

struct Button {
    float xMin, xMax;
    float yMin, yMax;
};

extern Screen currentScreen;
extern bool leftMouseDownLastFrame;

// OpenGL objekti
extern GLuint quadVAO;
extern GLuint quadVBO;
extern GLuint shaderProgram;

extern Button arrowRightTime;
extern Button arrowLeftHeart;
extern Button arrowRightHeart;
extern Button arrowLeftBattery;

extern int g_hours;
extern int g_minutes;
extern int g_seconds;

// osvezavanje sata
void initClock();
void updateClock();

// otkucaji srca
extern float g_bpm;          // trenutni BPM
extern float g_bpmTarget; 

//  update za HEART ekran
void initHeart();
void updateHeart(GLFWwindow* window);
void drawHeartScreen();

void initGL();                      // inicijalizacija OpenGL stanja, sejdera
void updateAndRender(GLFWwindow*);  // jedan frame: input + logika + crtanje

// Battery
void initBattery();
void updateBattery();
void drawBatteryScreen();

void initHeartCursor(GLFWwindow* window);
void destroyHeartCursor();

unsigned int compileShader(GLenum type, const char* source);
unsigned int createShader(const char* vsSource, const char* fsSource);
