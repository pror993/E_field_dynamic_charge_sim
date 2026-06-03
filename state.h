#pragma once

#include <glm/glm.hpp>
#include <deque>

struct StateFrame {
    float time;
    glm::vec2 position;
    glm::vec2 velocity;
    glm::vec2 acceleration;
};

struct FieldResult {
    glm::vec2 E; // Electric field in the XY plane
    float B;     // Magnetic field (scalar, representing the Z magnitude)
};

