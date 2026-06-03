#pragma once

// 1. The Speed of Light (Kept as is)
constexpr float SIMULATED_C = 300.0f;

// 2. The Physics Timestep (Significantly Reduced)
constexpr float PHYSICS_DT = 0.002f; 

// 3. History Window (Your math here is perfect)
constexpr float MAX_HISTORY_TIME = (800.0f * 1.41421356f) / SIMULATED_C; 

// 4. Electrostatic Constant (Drastically Reduced)
constexpr float K_E = 15000000.0f; 

// 5. NEW: The Universal Speed Limit
constexpr float MAX_VELOCITY = SIMULATED_C * 0.95f; 

// 6. NEW: Collision Softening 
constexpr float SOFTENING = 5.0f;