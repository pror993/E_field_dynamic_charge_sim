#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <algorithm>
#include <vector>

// --- IMGUI INCLUDES ---
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

// Custom headers
#include "config.h"
#include "state.h"
#include "charge.h"

// --- GLOBALS ---
std::vector<Charge> worldCharges;
double lastFrameTime = 0.0;
double accumulator = 0.0;

// (Keep your vertexShaderSource, fragmentShaderSource, and compileShaders() exactly as they were here)
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

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    // --- NEW: Block mouse clicks if ImGui is using the mouse ---
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) return; 

    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);
        glm::vec2 spawnPos(static_cast<float>(xpos), 800.0f - static_cast<float>(ypos));
        
        glm::vec2 initialVel(SIMULATED_C * 0.5f, 0.0f); 
        Charge newCharge;
        newCharge.charge = 1.0f;
        newCharge.mass = 1.0f;
        newCharge.radius = 5.0f;
        newCharge.recordState(glfwGetTime(), spawnPos, initialVel, glm::vec2(0.0f));
        
        worldCharges.push_back(newCharge);
    }
}

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800, 800, "Liénard-Wiechert Simulator", NULL, NULL);
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return -1;

    // --- IMGUI SETUP ---
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    unsigned int shaderProgram = compileShaders();
    glm::mat4 projection = glm::ortho(0.0f, 800.0f, 0.0f, 800.0f, -1.0f, 1.0f);
    glUseProgram(shaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    lastFrameTime = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        double currentTime = glfwGetTime();
        double frameTime = currentTime - lastFrameTime;
        lastFrameTime = currentTime;
        accumulator += frameTime;

        if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        // --- IMGUI NEW FRAME ---
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // --- PHYSICS UPDATE ---
        while (accumulator >= PHYSICS_DT) {
            std::vector<glm::vec2> newAccelerations(worldCharges.size(), glm::vec2(0.0f));

            for (size_t i = 0; i < worldCharges.size(); i++) {
                glm::vec2 netE(0.0f);
                float netB = 0.0f;
                for (size_t j = 0; j < worldCharges.size(); j++) {
                    if (i == j) continue; 
                    FieldResult res = worldCharges[j].calculateFieldAt(worldCharges[i].history.front().position, currentTime);
                    netE += res.E;
                    netB += res.B;
                }
                glm::vec2 v = worldCharges[i].history.front().velocity;
                glm::vec2 v_cross_B(v.y * netB, -v.x * netB); 
                glm::vec2 force = worldCharges[i].charge * (netE + v_cross_B);
                newAccelerations[i] = force / worldCharges[i].mass;
            }

            for (size_t i = 0; i < worldCharges.size(); i++) {
                StateFrame current = worldCharges[i].history.front();
                glm::vec2 newVel = current.velocity + (newAccelerations[i] * PHYSICS_DT);
                
                // Enforce the universal speed limit
                float speed = glm::length(newVel);
                if (speed > MAX_VELOCITY) {
                    newVel = (newVel / speed) * MAX_VELOCITY;
                }

                glm::vec2 newPos = current.position + (newVel * PHYSICS_DT);

                if (newPos.x > 800.0f) { newPos.x = 800.0f; newVel.x *= -1.0f; }
                if (newPos.x < 0.0f)   { newPos.x = 0.0f;   newVel.x *= -1.0f; }
                if (newPos.y > 800.0f) { newPos.y = 800.0f; newVel.y *= -1.0f; }
                if (newPos.y < 0.0f)   { newPos.y = 0.0f;   newVel.y *= -1.0f; }

                worldCharges[i].recordState(currentTime, newPos, newVel, newAccelerations[i]);
            }
            accumulator -= PHYSICS_DT;
        }

        glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        std::vector<float> renderData;
        double mouseX, mouseY;
        glfwGetCursorPos(window, &mouseX, &mouseY);
        glm::vec2 hoverPos(static_cast<float>(mouseX), 800.0f - static_cast<float>(mouseY));

        glm::vec2 probeE(0.0f);
        float probeB = 0.0f;

        for (auto& charge : worldCharges) {
            FieldResult res = charge.calculateFieldAt(hoverPos, currentTime);
            probeE += res.E;
            probeB += res.B;
        }

        float eMag = glm::length(probeE);
        if (eMag > 0.001f && !io.WantCaptureMouse) { // Don't draw probe under the UI
            glm::vec2 dir = probeE / eMag;
            float arrowLength = std::min(eMag * 0.05f, 150.0f); 
            glm::vec2 endPos = hoverPos + (dir * arrowLength);

            renderData.push_back(hoverPos.x); renderData.push_back(hoverPos.y);
            renderData.push_back(1.0f); renderData.push_back(1.0f); renderData.push_back(0.0f);
            renderData.push_back(endPos.x); renderData.push_back(endPos.y);
            renderData.push_back(1.0f); renderData.push_back(0.0f); renderData.push_back(0.0f);
        }

        for (auto& charge : worldCharges) {
            glm::vec2 p = charge.history.front().position;
            float r = charge.radius;
            renderData.push_back(p.x - r); renderData.push_back(p.y);
            renderData.push_back(1.0f); renderData.push_back(0.0f); renderData.push_back(0.0f);
            renderData.push_back(p.x + r); renderData.push_back(p.y);
            renderData.push_back(1.0f); renderData.push_back(0.0f); renderData.push_back(0.0f);
            renderData.push_back(p.x); renderData.push_back(p.y - r);
            renderData.push_back(1.0f); renderData.push_back(0.0f); renderData.push_back(0.0f);
            renderData.push_back(p.x); renderData.push_back(p.y + r);
            renderData.push_back(1.0f); renderData.push_back(0.0f); renderData.push_back(0.0f);
        }

        glUseProgram(shaderProgram);
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);

        if (!renderData.empty()) {
            glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(renderData.size() * sizeof(float)), renderData.data(), GL_DYNAMIC_DRAW);
            glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(renderData.size() / 5));
        }

        // --- BUILD IMGUI WINDOW ---
        ImGui::Begin("Simulation Controls");
        
        ImGui::Text("Field at Cursor:");
        ImGui::Text("E: (%.2f, %.2f) Mag: %.2f", probeE.x, probeE.y, eMag);
        ImGui::Text("B: %.2f", probeB);
        ImGui::Separator();
        
        ImGui::Text("Physics Parameters:");
        // The Sliders directly modify your global variables
        ImGui::SliderFloat("Speed of Light (c)", &SIMULATED_C, 100.0f, 1000.0f);
        ImGui::SliderFloat("Electrostatic K", &K_E, 1000.0f, 50000.0f);
        ImGui::SliderFloat("Softening", &SOFTENING, 0.1f, 20.0f);
        
        // Dynamically update max velocity based on current C
        MAX_VELOCITY = SIMULATED_C * 0.95f; 
        MAX_HISTORY_TIME = (800.0f * 1.41421356f) / SIMULATED_C;

        if (ImGui::Button("Clear All Particles")) {
            worldCharges.clear();
        }
        
        ImGui::End();

        // --- RENDER IMGUI ---
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // --- CLEANUP ---
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glDeleteBuffers(1, &VBO);
    glDeleteVertexArrays(1, &VAO);
    glDeleteProgram(shaderProgram);
    glfwTerminate();
    return 0;
}