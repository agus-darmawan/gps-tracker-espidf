#ifndef VEHICLE_PERFORMANCE_H
#define VEHICLE_PERFORMANCE_H

#include <stdint.h>
#include <stdbool.h>#

// Constants
#define GRAVITY 9.8
#define A_STANDARD 3.0
#define T_STANDARD 100
#define K_CONSTANT 0.0693  // k = ln(2)/10 ≈ 0.0693

// Performance data structure
typedef struct {
    // Cumulative distances for each component
    float s_rear_tire;
    float s_front_tire;
    float s_front_brake_pad;
    float s_rear_brake_pad;
    float s_chain_or_cvt;
    float s_engine_oil;
    float s_engine;  // Total distance
    float s_air_filter; // Total disatnce
    
    // Weight score
    // char weight_score[16];  // "ringan", "sedang", "berat"
    
    // Previous state
    float v_start;
    float last_distance;
    
    // Trip statistics
    float total_distance_km;
    float average_speed;
    float max_speed;
    int trip_count;
    
    // Active tracking
    bool is_tracking;
    char order_id[64];
} vehicle_performance_t;

// Function prototypes
void performance_init(void);
void performance_reset(void);
void performance_start_tracking(const char* order_id);
void performance_stop_tracking(void);
void performance_update(int s_real, int h, int v_end, float T_machine);
vehicle_performance_t performance_get_data(void);
const char* performance_get_weight_score(void);

// Helper functions
float rear_tire_force(float s_real, float h, float v_start, float v_end, int time);
float front_brake_work(float s_real, float h, float v_start, float v_end, int time, float mass, float wheelbase);
float rear_brake_work(float s_real, float h, float v_start, float v_end, int time, float mass, float wheelbase);
float count_s_oil(float s_real, float temp_machine);

#endif // VEHICLE_PERFORMANCE_H