#include <glm/glm.hpp>
#include "state.h"
#include "config.h"
#pragma once

#include <algorithm>
#include <cmath>
#include <deque>
#include <iterator>
#include <glm/glm.hpp>
#include "state.h"
#include "config.h"

struct Charge {
    float charge;
    float mass;
    float radius;
    std::deque<StateFrame> history;

    void recordState(float time, const glm::vec2& position, const glm::vec2& velocity, const glm::vec2& acceleration){
        StateFrame newFrame{time, position, velocity, acceleration};
        history.push_front(newFrame);
        // Remove old states that are outside the MAX_HISTORY_TIME window
        while (!history.empty() && (time - history.back().time) > MAX_HISTORY_TIME) {
            history.pop_back();                    
        }
    }
    StateFrame getRetardedState(glm::vec2 targetpos, float currentTime) {

        if (history.empty()) return StateFrame{currentTime, {0,0}, {0,0}, {0,0}};


        for(auto it = history.begin(); it != history.end(); ++it) {
            float distance = glm::distance(it->position, targetpos);
            float retardedTime = currentTime - distance / SIMULATED_C;
            if (it->time <= retardedTime) {

                if(it==history.begin()) {
                    return *it; // If it's the first frame, return it even if it's newer than the retarded time
                }
                auto newerit =std::prev(it);
                 
                float timeRange = newerit->time - it->time;
                if (timeRange <0.0001) {
                return *it;
                }

                float alpha = (retardedTime - it->time) / timeRange;

                StateFrame interpolatedFrame;
                interpolatedFrame.time = retardedTime;
                interpolatedFrame.position = glm::mix(it->position, newerit->position, alpha);
                interpolatedFrame.velocity = glm::mix(it->velocity, newerit->velocity, alpha);
                interpolatedFrame.acceleration = glm::mix(it->acceleration, newerit->acceleration, alpha);
                return interpolatedFrame;
            }
        }
        return history.back(); // If all frames are newer than the retarded time, return the oldest frame
    }
    
    FieldResult calculateFieldAt(glm::vec2 targetPos, float currentTime) {
        
        StateFrame retardedState = getRetardedState(targetPos, currentTime);
        // 2. Promote 2D vectors to 3D to easily handle cross products
        glm::vec3 r_vec = glm::vec3(targetPos - retardedState.position, 0.0f);
        float r_mag = glm::length(r_vec);

        if (r_mag < 0.001f) {
            return FieldResult{glm::vec2(0.0f, 0.0f), 0.0f}; // Avoid singularity
        }

        glm::vec3 r_hat = r_vec / r_mag;
        glm::vec3 v = glm::vec3(retardedState.velocity, 0.0f);
        glm::vec3 a = glm::vec3(retardedState.acceleration, 0.0f);

        //  Define the 'u' vector: u = c(r_hat) - v
        glm::vec3 u =(SIMULATED_C * r_hat - v);

        float r_dot_u = glm::dot(r_vec, u);

        if (std::abs(r_dot_u) < 0.0001f) return { glm::vec2(0.0f), 0.0f };

        float c2_minus_v2 = (SIMULATED_C * SIMULATED_C) - glm::dot(v, v);

        glm::vec3 u_cross_a = glm::cross(u, a);
        glm::vec3 r_cross_u_cross_a = glm::cross(r_vec, u_cross_a);

        // The Prefix: (q / 4*pi*eps0) * (r / (r_dot_u)^3)
        float prefix = (charge * K_E* r_mag) / std::pow(r_dot_u, 3.0f);

        glm::vec3 E_3D = prefix * ((u * c2_minus_v2) + r_cross_u_cross_a);

        glm::vec3 B_3D = (1.0f / SIMULATED_C) * glm::cross(r_hat, E_3D);

        return {glm::vec2(E_3D.x, E_3D.y), B_3D.z};

    }
};
