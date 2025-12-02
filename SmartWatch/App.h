#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>

// === Stanja ekrana na satu ===
enum class Screen {
    TIME,
    HEART,
    BATTERY
};

// Pravougaonik za klik (strelice kao "dugmići")
struct Button {
    float xMin, xMax;
    float yMin, yMax;
};

// === Globalno stanje (samo deklaracija, definicija u App.cpp) ===
extern Screen currentScreen;
extern bool leftMouseDownLastFrame;

// OpenGL objekti koje koristimo u više fajlova
extern GLuint quadVAO;
extern GLuint quadVBO;
extern GLuint shaderProgram;

// Dugmići (strelice) za svako stanje
extern Button arrowRightTime;
extern Button arrowLeftHeart;
extern Button arrowRightHeart;
extern Button arrowLeftBattery;

// === Sat (HH:MM:SS) ===
extern int g_hours;
extern int g_minutes;
extern int g_seconds;

// inicijalizacija i osvežavanje sata
void initClock();
void updateClock();

// === Heart (otkucaji srca, BPM) ===
extern float g_bpm;          // trenutni BPM
extern float g_bpmTarget;    // ka kojoj vrednosti teži BPM (trčanje / mirovanje)

// inicijalizacija i update za HEART ekran
void initHeart();
void updateHeart(GLFWwindow* window);
void drawHeartScreen();

// === Funkcije koje koristi main.cpp ===
void initGL();                      // inicijalizacija OpenGL stanja, šejdera, VAO/VBO
void updateAndRender(GLFWwindow*);  // jedan frame: input + logika + crtanje

// Battery
void initBattery();
void updateBattery();
void drawBatteryScreen();

void initHeartCursor(GLFWwindow* window);
void destroyHeartCursor();
