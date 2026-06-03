#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <algorithm>
#include <vector>

// Make sure your header file casings match what is on your disk
#include "config.h"
#include "state.h"
#include "charge.h"

// --- GLOBALS ---
std::vector<Charge> worldCharges;
double lastFrameTime = 0.0;
double accumulator = 0.0;

// --- SHADER SOURCE CODE ---
const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec3 aColor;
out vec3 vertexColor;
uniform mat4 projection;
void main() {
    gl_Position = projection * vec4(aPos, 0.0, 1.0);
    vertexColor = aColor;
}
)";

const char* fragmentShaderSource = R"(
#version 330 core
in vec3 vertexColor;
out vec4 FragColor;
void main() {
    FragColor = vec4(vertexColor, 1.0);
}
)";

// --- UTILITY: COMPILE SHADERS ---
unsigned int compileShaders() {
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return shaderProgram;
}

// --- CALLBACKS ---
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);

        // Convert screen coordinates to world coordinates (Y is inverted)
        glm::vec2 spawnPos(static_cast<float>(xpos), 800.0f - static_cast<float>(ypos));
        
        // Spawn a charge: q = 1.0, initial velocity = rightward at 0.5c
        glm::vec2 initialVel(SIMULATED_C * 0.5f, 0.0f); 
        Charge newCharge;
        newCharge.charge = 1.0f;
        newCharge.mass = 1.0f;
        newCharge.radius = 5.0f;
        newCharge.recordState(glfwGetTime(), spawnPos, initialVel, glm::vec2(0.0f));
        
        worldCharges.push_back(newCharge);
        std::cout << "Spawned charge at: " << spawnPos.x << ", " << spawnPos.y << "\n";
    }
}

// --- MAIN ---
int main() {
    // 1. GLFW Setup
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800, 800, "Liénard-Wiechert Simulator", NULL, NULL);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);

    // 2. GLAD Setup
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD\n";
        return -1;
    }

    // 3. Shader & Rendering Setup
    unsigned int shaderProgram = compileShaders();
    
    // Orthographic projection matching our 800x800 world
    glm::mat4 projection = glm::ortho(0.0f, 800.0f, 0.0f, 800.0f, -1.0f, 1.0f);
    glUseProgram(shaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    // Dynamic VBO/VAO for lines
    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    // 5 floats per vertex: x, y, r, g, b
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // 4. Main Loop
    lastFrameTime = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        // --- TIMING & INPUT ---
        double currentTime = glfwGetTime();
        double frameTime = currentTime - lastFrameTime;
        lastFrameTime = currentTime;
        accumulator += frameTime;

        if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        // --- PHYSICS UPDATE (Fixed Timestep) ---
        while (accumulator >= PHYSICS_DT) {
            
            // 1. Calculate net forces for all particles BEFORE moving them
            std::vector<glm::vec2> newAccelerations(worldCharges.size(), glm::vec2(0.0f));

            for (size_t i = 0; i < worldCharges.size(); i++) {
                glm::vec2 netE(0.0f);
                float netB = 0.0f;

                // Sum fields from all OTHER charges
                for (size_t j = 0; j < worldCharges.size(); j++) {
                    if (i == j) continue; 

                    FieldResult res = worldCharges[j].calculateFieldAt(
                        worldCharges[i].history.front().position, currentTime
                    );
                    netE += res.E;
                    netB += res.B;
                }

                // Calculate Lorentz Force: F = q(E + v x B)
                glm::vec2 v = worldCharges[i].history.front().velocity;
                glm::vec2 v_cross_B(v.y * netB, -v.x * netB); 
                
                glm::vec2 force = worldCharges[i].charge * (netE + v_cross_B);
                newAccelerations[i] = force / worldCharges[i].mass;
            }

            // 2. Apply integration to update positions
            for (size_t i = 0; i < worldCharges.size(); i++) {
                StateFrame current = worldCharges[i].history.front();
                
                // Update velocity and position
                glm::vec2 newVel = current.velocity + (newAccelerations[i] * PHYSICS_DT);
                glm::vec2 newPos = current.position + (newVel * PHYSICS_DT);

                // FIXED: Clamp positions to prevent wall-sticking
                if (newPos.x > 800.0f) { newPos.x = 800.0f; newVel.x *= -1.0f; }
                if (newPos.x < 0.0f)   { newPos.x = 0.0f;   newVel.x *= -1.0f; }
                if (newPos.y > 800.0f) { newPos.y = 800.0f; newVel.y *= -1.0f; }
                if (newPos.y < 0.0f)   { newPos.y = 0.0f;   newVel.y *= -1.0f; }

                worldCharges[i].recordState(currentTime, newPos, newVel, newAccelerations[i]);
            }
            accumulator -= PHYSICS_DT;
        }

        // --- RENDER PREP ---
        glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        std::vector<float> renderData;

        // --- HOVER PROBE (Dynamic Cursor Arrow) ---
        double mouseX, mouseY;
        glfwGetCursorPos(window, &mouseX, &mouseY);
        
        // Convert to our world coordinates (inverted Y)
        glm::vec2 hoverPos(static_cast<float>(mouseX), 800.0f - static_cast<float>(mouseY));

        glm::vec2 probeE(0.0f);
        float probeB = 0.0f;

        // Calculate total field at mouse cursor
        for (auto& charge : worldCharges) {
            FieldResult res = charge.calculateFieldAt(hoverPos, currentTime);
            probeE += res.E;
            probeB += res.B;
        }

        // Draw the vector if the field is strong enough
        float eMag = glm::length(probeE);
        if (eMag > 0.0001f) {
            glm::vec2 dir = probeE / eMag;
            
            // Visual scaling: tweak this multiplier so the arrow looks good on your screen
            float arrowLength = std::min(eMag * 0.05f, 200.0f); // Cap length at 200px
            glm::vec2 endPos = hoverPos + (dir * arrowLength);

            // Base of arrow at cursor (Yellow)
            renderData.push_back(hoverPos.x); renderData.push_back(hoverPos.y);
            renderData.push_back(1.0f); renderData.push_back(1.0f); renderData.push_back(0.0f);
            
            // Tip of arrow pointing in field direction (Red)
            renderData.push_back(endPos.x); renderData.push_back(endPos.y);
            renderData.push_back(1.0f); renderData.push_back(0.0f); renderData.push_back(0.0f);
        }

        // --- DRAW PARTICLES (Restored) ---
        for (auto& charge : worldCharges) {
            glm::vec2 p = charge.history.front().position;
            float r = charge.radius;
            // Horizontal line of the cross (Red)
            renderData.push_back(p.x - r); renderData.push_back(p.y);
            renderData.push_back(1.0f); renderData.push_back(0.0f); renderData.push_back(0.0f);
            renderData.push_back(p.x + r); renderData.push_back(p.y);
            renderData.push_back(1.0f); renderData.push_back(0.0f); renderData.push_back(0.0f);
            // Vertical line of the cross (Red)
            renderData.push_back(p.x); renderData.push_back(p.y - r);
            renderData.push_back(1.0f); renderData.push_back(0.0f); renderData.push_back(0.0f);
            renderData.push_back(p.x); renderData.push_back(p.y + r);
            renderData.push_back(1.0f); renderData.push_back(0.0f); renderData.push_back(0.0f);
        }

        // --- DRAW ---
        glUseProgram(shaderProgram);
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);

        if (!renderData.empty()) {
            glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(renderData.size() * sizeof(float)), renderData.data(), GL_DYNAMIC_DRAW);
            glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(renderData.size() / 5));
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // 5. Cleanup
    glDeleteBuffers(1, &VBO);
    glDeleteVertexArrays(1, &VAO);
    glDeleteProgram(shaderProgram);
    glfwTerminate();
    return 0;
}