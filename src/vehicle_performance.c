#include "vehicle_performance.h"
#include "esp_log.h"
#include <math.h>
#include <string.h>

static const char *TAG = "PERFORMANCE";
static vehicle_performance_t perf_data = {0};
static const int DELTA_T = 3;  // Time interval in seconds

/**
 * Calculate rear tire force based on acceleration and elevation.
 */
int rear_tire_force(int s_real, int h, int v_start, int v_end, int t) {
    double result = ((((v_end - v_start) / (double)t) + (GRAVITY * h / s_real)) / A_STANDARD * s_real);
    return (int)round(fabs(result));
}

/**
 * Calculate brake work based on deceleration and elevation.
 */
int brake_work(int s_real, int h, int v_start, int v_end, int t) {
    double result = ((((v_start - v_end) / (double)t) - (GRAVITY * h / s_real)) / A_STANDARD * s_real);
    return (int)round(fabs(result));
}

/**
 * Calculate engine oil wear based on temperature.
 */
int count_s_oil(int s_real, float T_machine) {
    double result = s_real * exp(K_CONSTANT * (T_machine - T_STANDARD));
    return (int)round(fabs(result));
}

/**
 * Determine weight score based on total load.
 */
static void update_weight_score(void) {
    // Calculate average load per meter
    int total_load = perf_data.s_rear_tire + perf_data.s_front_tire + 
                     perf_data.s_brake_pad + perf_data.s_chain_or_cvt;
    
    if (perf_data.s_engine == 0) {
        strcpy(perf_data.weight_score, "ringan");
        return;
    }
    
    float load_ratio = (float)total_load / perf_data.s_engine;
    
    if (load_ratio < 2.0) {
        strcpy(perf_data.weight_score, "ringan");
    } else if (load_ratio < 4.0) {
        strcpy(perf_data.weight_score, "sedang");
    } else {
        strcpy(perf_data.weight_score, "berat");
    }
}

/**
 * Initialize performance tracking system.
 */
void performance_init(void) {
    memset(&perf_data, 0, sizeof(vehicle_performance_t));
    strcpy(perf_data.weight_score, "ringan");
    perf_data.is_tracking = false;
    ESP_LOGI(TAG, "Performance tracking initialized");
}

/**
 * Reset all performance counters.
 */
void performance_reset(void) {
    perf_data.s_rear_tire = 0;
    perf_data.s_front_tire = 0;
    perf_data.s_brake_pad = 0;
    perf_data.s_chain_or_cvt = 0;
    perf_data.s_engine_oil = 0;
    perf_data.s_engine = 0;
    perf_data.v_start = 0;
    perf_data.total_distance_km = 0;
    perf_data.average_speed = 0;
    perf_data.max_speed = 0;
    perf_data.trip_count = 0;
    strcpy(perf_data.weight_score, "ringan");
    memset(perf_data.order_id, 0, sizeof(perf_data.order_id));
    ESP_LOGI(TAG, "Performance counters reset");
}

/**
 * Start tracking for a new rental order.
 */
void performance_start_tracking(const char* order_id) {
    performance_reset();
    if (order_id != NULL) {
        strncpy(perf_data.order_id, order_id, sizeof(perf_data.order_id) - 1);
    }
    perf_data.is_tracking = true;
    ESP_LOGI(TAG, "Started tracking for order: %s", perf_data.order_id);
}

/**
 * Stop tracking and finalize data.
 */
void performance_stop_tracking(void) {
    perf_data.is_tracking = false;
    update_weight_score();
    
    // Calculate final statistics
    if (perf_data.trip_count > 0) {
        perf_data.average_speed = perf_data.average_speed / perf_data.trip_count;
    }
    
    ESP_LOGI(TAG, "Stopped tracking. Total distance: %.2f km", perf_data.total_distance_km);
    ESP_LOGI(TAG, "Weight score: %s", perf_data.weight_score);
}

/**
 * Update performance data with new measurement.
 */
void performance_update(int s_real, int h, int v_end, float T_machine) {
    if (!perf_data.is_tracking) {
        return;
    }
    
    int v_start = perf_data.v_start;
    
    // Initialize deltas
    int delta_rear = s_real;
    int delta_front = s_real;
    int delta_brake = 0;
    int delta_chain = s_real;
    
    // Flat road (h == 0)
    if (h == 0) {
        if (v_end > v_start) {
            // Acceleration
            delta_rear = rear_tire_force(s_real, h, v_start, v_end, DELTA_T);
            delta_chain = rear_tire_force(s_real, h, v_start, v_end, DELTA_T);
        } else if (v_end < v_start) {
            // Deceleration with braking
            int work = brake_work(s_real, h, v_start, v_end, DELTA_T);
            delta_rear = (int)(0.3 * work);
            delta_front = (int)(0.7 * work);
            delta_brake = work;
            delta_chain = (int)(0.3 * work);
        }
    }
    // Uphill (h > 0)
    else if (h > 0) {
        if (v_end >= v_start) {
            // Acceleration or constant speed uphill
            delta_rear = rear_tire_force(s_real, h, v_start, v_end, DELTA_T);
            delta_chain = rear_tire_force(s_real, h, v_start, v_end, DELTA_T);
        } else {
            // Deceleration uphill (without brakes)
            delta_rear = rear_tire_force(s_real, h, v_start, v_end, DELTA_T);
            delta_chain = rear_tire_force(s_real, h, v_start, v_end, DELTA_T);
        }
    }
    // Downhill (h < 0)
    else {
        if (v_end > v_start) {
            // Acceleration downhill
            delta_rear = rear_tire_force(s_real, h, v_start, v_end, DELTA_T);
            delta_chain = rear_tire_force(s_real, h, v_start, v_end, DELTA_T);
        } else {
            // Deceleration or constant downhill (using brakes)
            int work = brake_work(s_real, h, v_start, v_end, DELTA_T);
            delta_rear = (int)(0.3 * work);
            delta_front = (int)(0.7 * work);
            delta_brake = work;
            delta_chain = (int)(0.3 * work);
        }
    }
    
    // Update cumulative values
    perf_data.s_rear_tire += delta_rear;
    perf_data.s_front_tire += delta_front;
    perf_data.s_brake_pad += delta_brake;
    perf_data.s_chain_or_cvt += delta_chain;
    perf_data.s_engine_oil += count_s_oil(s_real, T_machine);
    perf_data.s_engine += s_real;
    
    // Update statistics
    perf_data.total_distance_km = perf_data.s_engine / 1000.0;
    perf_data.average_speed += v_end;
    perf_data.trip_count++;
    
    if (v_end > perf_data.max_speed) {
        perf_data.max_speed = v_end;
    }
    
    // Update starting velocity for next iteration
    perf_data.v_start = v_end;
    
    ESP_LOGD(TAG, "Updated: distance=%d, rear=%d, front=%d, brake=%d, chain=%d, oil=%d",
             s_real, delta_rear, delta_front, delta_brake, delta_chain, 
             count_s_oil(s_real, T_machine));
}

/**
 * Get current performance data.
 */
vehicle_performance_t performance_get_data(void) {
    return perf_data;
}

/**
 * Get current weight score.
 */
const char* performance_get_weight_score(void) {
    return perf_data.weight_score;
}