#ifndef VEHICLE_PERFORMANCE_H
#define VEHICLE_PERFORMANCE_H

#include <stdint.h>
#include <stdbool.h>

// Constants
#define GRAVITY 9.8
#define A_STANDARD 3.0
#define T_STANDARD 100
#define K_CONSTANT 0.0693  // k = ln(2)/10 ≈ 0.0693

// Performance data structure
typedef struct {
    // Cumulative distances for each component
    int s_rear_tire;
    int s_front_tire;
    int s_brake_pad;
    int s_chain_or_cvt;
    int s_engine_oil;
    int s_engine;  // Total distance
    
    // Weight score
    char weight_score[16];  // "ringan", "sedang", "berat"
    
    // Previous state
    int v_start;
    int last_distance;
    
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
int rear_tire_force(int s_real, int h, int v_start, int v_end, int t);
int brake_work(int s_real, int h, int v_start, int v_end, int t);
int count_s_oil(int s_real, float T_machine);

#endif // VEHICLE_PERFORMANCE_H