# Testing Guide - Vehicle GPS Tracker

## 🧪 Testing Overview

This guide covers comprehensive testing procedures for the Vehicle GPS Tracker system, from individual component testing to full system integration tests.

## 📋 Test Prerequisites

### Hardware Setup
- ✅ All sensors properly wired
- ✅ ESP32 powered and programmed
- ✅ GPS module with clear sky view
- ✅ Stable WiFi connection
- ✅ RabbitMQ broker accessible

### Software Setup
```bash
# Install Python dependencies
pip install -r requirements.txt

# Verify installations
python --version  # 3.8+
pio --version     # Latest
```

## 🔬 Unit Tests

### 1. WiFi Connection Test

**Objective**: Verify WiFi connectivity

```bash
# Monitor serial output
pio run --target monitor
```

**Expected Output**:
```
[WIFI_MANAGER] WiFi init finished. Connecting to YourSSID
[WIFI_MANAGER] Connected to WiFi with IP: 192.168.1.XXX
```

**Pass Criteria**:
- ✅ Connects within 30 seconds
- ✅ IP address displayed
- ✅ No disconnect errors

### 2. GPS Module Test

**Objective**: Verify GPS data reception and parsing

**Test Steps**:
1. Place GPS module outdoors or near window
2. Monitor serial output for GPS data
3. Wait for valid fix (may take 30-60 seconds on cold start)

**Expected Output**:
```
[GPS] GPS task started
[GPS] GPS initialized on UART2 (TX: GPIO17, RX: GPIO16)
```

**Pass Criteria**:
- ✅ GPS fix acquired within 60 seconds
- ✅ Latitude/Longitude values reasonable
- ✅ Altitude data present
- ✅ No checksum errors

**Troubleshooting**:
```bash
# Check UART communication
# Add to main.c temporarily:
ESP_LOGI("GPS", "Raw: %.*s", len, data);
```

### 3. Temperature Sensor Test

**Objective**: Verify MAX6675 thermocouple readings

**Test Code Addition** (in main.c):
```c
// In vehicle_tracking_task, add:
float temp = max6675_read_temperature();
ESP_LOGI("TEST", "Temperature: %.2f°C", temp);
```

**Expected Output**:
```
[MAX6675] MAX6675 initialized successfully
[MAX6675] Temperature: 25.50°C
```

**Pass Criteria**:
- ✅ Temperature reads between 0-300°C
- ✅ No SPI communication errors
- ✅ Values change when heat applied

### 4. IMU Sensor Test

**Objective**: Verify MPU6050 accelerometer and gyroscope

**Test Code Addition**:
```c
mpu6050_data_t mpu_data;
if (mpu6050_read_data(&mpu_data) == ESP_OK) {
    ESP_LOGI("TEST", "Pitch: %.2f, Roll: %.2f", mpu_data.pitch, mpu_data.roll);
}
```

**Expected Output**:
```
[MPU6050] MPU6050 initialized successfully
[TEST] Pitch: 0.50, Roll: -1.20
```

**Pass Criteria**:
- ✅ Pitch/Roll change when tilted
- ✅ Values between -90 and +90 degrees
- ✅ No I2C errors

## 🔗 Integration Tests

### 1. Web Configuration Test

**Objective**: Verify web server and NVS storage

**Test Steps**:
1. Fresh flash ESP32 (erase flash first)
   ```bash
   pio run --target erase
   pio run --target upload
   ```

2. Find ESP32 IP in serial monitor

3. Open browser: `http://[ESP32_IP]/`

4. Enter vehicle ID: "TEST001"

5. Click "Save Configuration"

6. Wait for restart

**Expected Behavior**:
```
[WEB_CONFIG] HTTP server started successfully
[WEB_CONFIG] Configuration saved to NVS
[MAIN] Vehicle ID: TEST001
```

**Pass Criteria**:
- ✅ Web page loads correctly
- ✅ Configuration saves successfully
- ✅ Device restarts with saved ID
- ✅ Subsequent boots skip configuration

### 2. MQTT Connection Test

**Objective**: Verify MQTT broker connectivity

**Test Steps**:
1. Ensure RabbitMQ broker is running
2. Monitor serial output
3. Check for MQTT connection messages

**Expected Output**:
```
[MQTT_VEHICLE] MQTT client initialized for vehicle: TEST001
[MQTT_VEHICLE] Connected to MQTT broker
[MQTT_VEHICLE] Subscribed to control topics
[MQTT_VEHICLE] Published registration for vehicle: TEST001
```

**Pass Criteria**:
- ✅ Connects within 5 seconds
- ✅ Subscriptions successful
- ✅ Registration message sent
- ✅ No disconnection errors

**Verify on RabbitMQ**:
```bash
# Check if queues have bindings
curl -u backend:backend123 http://103.175.219.138:15672/api/queues
```

### 3. Real-time Data Publishing Test

**Objective**: Verify all data streams

**Test Steps**:
1. Start monitor script:
   ```bash
   python monitor_vehicle.py TEST001
   ```

2. Wait for data updates

**Expected Monitor Output**:
```
📍 LOCATION UPDATE #1
   Coordinates: -6.208800, 106.845600
   Altitude: 10.50m

🟢 STATUS UPDATE #1
   Active: False | Locked: True 🔒

🔋 BATTERY UPDATE #1
   Voltage: 12.60V | Level: 100.00%
```

**Pass Criteria**:
- ✅ Location updates every 5 seconds
- ✅ Status updates every 5 seconds
- ✅ Battery updates every 10 seconds
- ✅ All data fields populated correctly

## 🚗 Functional Tests

### Test Scenario 1: Complete Rental Cycle

**Objective**: Test full start-to-end rental flow

**Test Steps**:

1. **Initial State**
   ```bash
   # Verify vehicle is locked
   # Check monitor output
   ```
   Expected: `Locked: True`, `Active: False`

2. **Start Rental**
   ```bash
   python test_commands.py TEST001 start_rent ORD-TEST-001
   ```
   
   **Expected**:
   - ✅ Status changes to: `Locked: False`, `Active: True`
   - ✅ Performance tracking begins
   - ✅ Serial shows: "Started tracking for order: ORD-TEST-001"

3. **Simulate Movement** (manually move GPS or use test data)
   - Move vehicle to generate location changes
   - Tilt device to simulate elevation changes
   - Apply heat to thermocouple

   **Expected**:
   - ✅ Location coordinates change
   - ✅ Performance data updates in serial log
   - ✅ Speed calculations occur

4. **End Rental**
   ```bash
   python test_commands.py TEST001 end_rent
   ```
   
   **Expected**:
   - ✅ Status changes to: `Locked: True`, `Active: False`
   - ✅ Performance report published
   - ✅ Monitor shows complete performance summary

**Performance Report Validation**:
```
📊 PERFORMANCE REPORT
Order ID: ORD-TEST-001
Distance: X.XX km
Component Wear: [All values > 0]
```

### Test Scenario 2: Emergency Kill

**Objective**: Test remote vehicle shutdown

**Test Steps**:

1. **Start Rental First**
   ```bash
   python test_commands.py TEST001 start_rent ORD-KILL-TEST
   ```

2. **Send Kill Command**
   ```bash
   python test_commands.py TEST001 kill
   ```
   
   **Expected Serial Output**:
   ```
   [MQTT_VEHICLE] Kill vehicle scheduled (waiting for low speed)
   ```

3. **Wait for Speed Reduction** (simulate by staying still or moving slowly)

   **Expected**:
   - ✅ When speed < 10 km/h: Vehicle stops
   - ✅ Serial shows: "Kill executed (speed < 10)"
   - ✅ Status: `Killed: True`, `Active: False`, `Locked: True`

**Pass Criteria**:
- ✅ Kill schedules correctly
- ✅ Executes only when safe (speed < 10)
- ✅ Status updates immediately

### Test Scenario 3: Performance Calculation Validation

**Objective**: Verify component wear calculations

**Test Setup**:
Create controlled test conditions:
- Flat road: h = 0
- Constant acceleration: v_start = 0, v_end = 20 km/h
- Distance: 100m
- Temperature: 85°C

**Expected Calculations**:
```
Rear Tire Force = ((20-0)/3 + 0) / 3.0 * 100 = 222m
Chain Force = 222m
Oil Wear = 100 * exp(0.0693 * (85-100)) = ~42m
```

**Verification**:
```bash
# Add debug logging in vehicle_performance.c
ESP_LOGI(TAG, "Calculated: rear=%d, chain=%d, oil=%d", 
         delta_rear, delta_chain, count_s_oil(s_real, T_machine));
```

**Pass Criteria**:
- ✅ Calculations match expected formulas
- ✅ Weight score updates correctly
- ✅ Statistics accumulate properly

## 📊 Performance Tests

### 1. Memory Usage Test

**Objective**: Ensure system runs within memory constraints

**Test Code**:
```c
// Add to main loop
ESP_LOGI("PERF", "Free heap: %lu bytes, Min free: %lu bytes", 
         esp_get_free_heap_size(), esp_get_minimum_free_heap_size());
```

**Pass Criteria**:
- ✅ Free heap stays above 50KB
- ✅ No heap fragmentation warnings
- ✅ Minimum free heap stable over time

### 2. Network Reliability Test

**Objective**: Test system under poor network conditions

**Test Steps**:
1. Run system normally for 5 minutes
2. Disconnect WiFi router
3. Wait 2 minutes
4. Reconnect WiFi router

**Expected Behavior**:
- ✅ System attempts reconnection
- ✅ MQTT reconnects automatically
- ✅ Data queues and sends after reconnection
- ✅ No crashes or reboots

### 3. Long-term Stability Test

**Objective**: Verify 24+ hour operation

**Test Steps**:
1. Start monitor script
2. Leave system running for 24 hours
3. Log all updates

**Pass Criteria**:
- ✅ No crashes or reboots
- ✅ Memory usage remains stable
- ✅ All sensors continue reporting
- ✅ MQTT connection maintained

## 🔍 Diagnostic Commands

### Enable Debug Logging

Add to `platformio.ini`:
```ini
build_flags = 
    -DCORE_DEBUG_LEVEL=5  ; Maximum verbosity
    -DLOG_LOCAL_LEVEL=ESP_LOG_VERBOSE
```

### I2C Scanner Test

```c
// Add temporary test function
void scan_i2c(void) {
    for (uint8_t addr = 1; addr < 127; addr++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);
        
        esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, 50 / portTICK_PERIOD_MS);
        i2c_cmd_link_delete(cmd);
        
        if (ret == ESP_OK) {
            ESP_LOGI("I2C", "Found device at address: 0x%02X", addr);
        }
    }
}
```

### MQTT Traffic Monitor

```bash
# Monitor all MQTT traffic on broker
mosquitto_sub -h 103.175.219.138 -p 1883 -u backend -P backend123 -t '#' -v
```

## ✅ Test Results Template

### Test Run Information
- **Date**: ___________
- **Tester**: ___________
- **Firmware Version**: ___________
- **Hardware Revision**: ___________

### Unit Tests
- [ ] WiFi Connection: PASS / FAIL
- [ ] GPS Module: PASS / FAIL
- [ ] Temperature Sensor: PASS / FAIL
- [ ] IMU Sensor: PASS / FAIL

### Integration Tests
- [ ] Web Configuration: PASS / FAIL
- [ ] MQTT Connection: PASS / FAIL
- [ ] Real-time Publishing: PASS / FAIL

### Functional Tests
- [ ] Complete Rental Cycle: PASS / FAIL
- [ ] Emergency Kill: PASS / FAIL
- [ ] Performance Calculation: PASS / FAIL

### Performance Tests
- [ ] Memory Usage: PASS / FAIL
- [ ] Network Reliability: PASS / FAIL
- [ ] Long-term Stability: PASS / FAIL

### Notes
___________________________________________________________
___________________________________________________________
___________________________________________________________

## 🐛 Common Test Failures

### GPS Not Publishing Location
**Symptoms**: Monitor shows no location updates

**Diagnosis**:
1. Check serial: GPS task running?
2. Verify GPS has fix: `valid: true`
3. Check MQTT connection

**Fix**:
- Wait 60s for GPS cold start
- Move to clear sky view
- Check UART wiring

### Performance Report Missing Data
**Symptoms**: Report shows zeros for component wear

**Diagnosis**:
1. Was tracking started with start_rent?
2. Was there actual movement?
3. Check temperature sensor readings

**Fix**:
- Always send start_rent before testing
- Ensure GPS coordinates change
- Verify sensors are working

### MQTT Disconnections
**Symptoms**: Frequent reconnection messages

**Diagnosis**:
1. Check WiFi signal strength
2. Verify broker is stable
3. Check keepalive settings

**Fix**:
```c
// Increase keepalive in mqtt_vehicle_client.h
#define MQTT_KEEPALIVE 120  // was 60
```

## 📈 Automated Testing

### Unit Test Framework Setup

```bash
# Install Unity testing framework
pio test
```

### Example Unit Test

```c
// test/test_performance.c
#include <unity.h>
#include "vehicle_performance.h"

void test_rear_tire_calculation(void) {
    int result = rear_tire_force(100, 0, 0, 20, 3);
    TEST_ASSERT_EQUAL_INT(222, result);
}

void setUp(void) {
    performance_init();
}

void tearDown(void) {
    performance_reset();
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_rear_tire_calculation);
    return UNITY_END();
}
```

## 🎯 Test Coverage Goals

- **Unit Tests**: 80%+ code coverage
- **Integration Tests**: All major flows tested
- **Performance Tests**: 24hr stability verified
- **Field Tests**: Real-world validation complete

## 📞 Reporting Issues

When reporting test failures:
1. Provide full serial log output
2. Include test scenario details
3. Note environmental conditions
4. Attach monitor script output if applicable
5. Specify firmware version and hardware setup

---

**Remember**: Thorough testing prevents costly field failures! 🛡️