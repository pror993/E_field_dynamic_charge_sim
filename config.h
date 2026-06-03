#pragma once

// Using inline allows these to be globally modified by ImGui
inline float SIMULATED_C = 300.0f;
inline float PHYSICS_DT = 0.002f; 
inline float MAX_HISTORY_TIME = (800.0f * 1.41421356f) / 300.0f; 
inline float K_E = 15000.0f; 
inline float MAX_VELOCITY = 300.0f * 0.95f; 
inline float SOFTENING = 5.0f;