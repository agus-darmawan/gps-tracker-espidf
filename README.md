# Vehicle GPS Tracker - ESP32

ESP32-based GPS tracker system for vehicle rental management with real-time monitoring, performance tracking, and remote control capabilities via MQTT/RabbitMQ.

## 🎯 Features

### Real-time Monitoring
- **GPS Location Tracking**: Real-time vehicle position with latitude/longitude/altitude
- **Status Monitoring**: Vehicle lock status, active status, and kill status
- **Battery Monitoring**: Voltage and battery level tracking
- **Temperature Monitoring**: Engine temperature via MAX6675 thermocouple sensor

### Performance Tracking
- **Component Wear Calculation**: Tracks wear on:
  - Front and rear tires
  - Brake pads
  - Engine oil
  - Chain/CVT
  - Engine (total distance)
- **Weight Score**: Automatic classification (ringan/sedang/berat) based on usage patterns
- **Trip Statistics**: Distance travelled, average speed, max speed

### Remote Control
- **Start Rent**: Unlock vehicle and begin performance tracking
- **End Rent**: Lock vehicle and generate performance report
- **Kill Vehicle**: Emergency vehicle shutdown (executes when speed < 10 km/h)

### Configuration
- **Web Interface**: Initial setup via WiFi hotspot for Vehicle ID configuration
- **Persistent Storage**: Configuration saved to NVS (Non-Volatile Storage)

## 📋 Hardware Requirements

### ESP32 Development Board
- ESP32-DOIT-DEVKIT-V1 or compatible

### Sensors
1. **GPS Module** (UART)
   - TX: GPIO17
   - RX: GPIO16
   - Baud rate: 9600

2. **MAX6675 Thermocouple** (SPI)
   - CLK: GPIO18
   - MISO: GPIO19
   - CS: GPIO5

3. **MPU6050 IMU** (I2C)
   - SCL: GPIO22
   - SDA: GPIO21

## 🚀 Getting Started

### 1. Prerequisites
- [PlatformIO](https://platformio.org/) installed
- ESP32 development board
- USB cable for programming

### 2. Configuration

Edit `src/main.c` to set your WiFi credentials:
```c
#define WIFI_SSID "your_wifi_ssid"
#define WIFI_PASS "your_wifi_password"
```

### 3. Build and Upload

```bash
# Build project
pio run

# Upload to ESP32
pio run --target upload

# Monitor serial output
pio run --target monitor
```

### 4. Initial Setup

1. **Connect to WiFi**: ESP32 will connect to the configured WiFi network
2. **Configure Vehicle ID**: 
   - Open browser and navigate to ESP32's IP address (displayed in serial monitor)
   - Enter vehicle ID (e.g., plate number: "B1234ABC")
   - Save configuration
3. **System Ready**: ESP32 will restart and begin normal operation

## 📡 MQTT Topics

### Published Topics (by ESP32)

#### Registration
```
Topic: registration.new
Payload: {
  "vehicle_id": "B1234ABC"
}
```

#### Real-time Location
```
Topic: realtime.location.{vehicle_id}
Payload: {
  "vehicle_id": "B1234ABC",
  "latitude": -6.2088,
  "longitude": 106.8456,
  "altitude": 10.5,
  "timestamp": "2025-11-12T10:30:00.000Z"
}
Interval: 5 seconds
```

#### Real-time Status
```
Topic: realtime.status.{vehicle_id}
Payload: {
  "vehicle_id": "B1234ABC",
  "is_active": true,
  "is_locked": false,
  "is_killed": false,
  "timestamp": "2025-11-12T10:30:00.000Z"
}
Interval: 5 seconds
```

#### Real-time Battery
```
Topic: realtime.battery.{vehicle_id}
Payload: {
  "vehicle_id": "B1234ABC",
  "device_voltage": 12.6,
  "device_battery_level": 95.5,
  "timestamp": "2025-11-12T10:30:00.000Z"
}
Interval: 10 seconds
```

#### Performance Report (sent on end_rent)
```
Topic: report.performance.{vehicle_id}
Payload: {
  "vehicle_id": "B1234ABC",
  "order_id": "ORD-123456",
  "weight_score": "sedang",
  "front_tire": 2500,
  "rear_tire": 3200,
  "brake_pad": 1800,
  "engine_oil": 2100,
  "chain_or_cvt": 3000,
  "engine": 2500,
  "distance_travelled": 2.5,
  "average_speed": 35.6,
  "max_speed": 65.0,
  "timestamp": "2025-11-12T11:00:00.000Z"
}
```

### Subscribed Topics (commands to ESP32)

#### Start Rent
```
Topic: control.start_rent.{vehicle_id}
Payload: {
  "order_id": "ORD-123456"
}
```

#### End Rent
```
Topic: control.end_rent.{vehicle_id}
Payload: {}
```

#### Kill Vehicle
```
Topic: control.kill_vehicle.{vehicle_id}
Payload: {}
```

## 🧮 Performance Calculation Algorithm

The system uses physics-based calculations to estimate component wear:

### Variables
- `s_real`: Distance traveled (meters)
- `h`: Elevation change (meters)
- `v_start`: Starting velocity (km/h)
- `v_end`: Ending velocity (km/h)
- `t`: Time interval (seconds, default: 3)
- `T_machine`: Engine temperature (°C)

### Formulas

#### Rear Tire Force
```
F_rear = ((a + g*h/s) / a_std) * s_real
where:
  a = (v_end - v_start) / t
  g = 9.8 m/s²
  a_std = 3.0 m/s² (standard acceleration)
```

#### Brake Work
```
W_brake = ((|a| - g*h/s) / a_std) * s_real
Distribution: 70% front, 30% rear
```

#### Engine Oil Wear
```
s_oil = s_real * exp(k * (T - T_std))
where:
  k = 0.0693 (ln(2)/10)
  T_std = 100°C
```

### Conditions

**Flat Road (h = 0)**
- Accelerating: Rear tire + chain wear
- Decelerating: Brake wear (70% front, 30% rear)
- Constant speed: Normal tire wear

**Uphill (h > 0)**
- Accelerating/Constant: Higher rear tire + chain wear
- Decelerating: Natural slowdown (no brake)

**Downhill (h < 0)**
- Accelerating: Normal rear tire + chain wear
- Decelerating: Heavy brake wear + reduced tire wear

### Weight Score Classification
- **Ringan** (Light): load_ratio < 2.0
- **Sedang** (Medium): 2.0 ≤ load_ratio < 4.0
- **Berat** (Heavy): load_ratio ≥ 4.0

## 🔧 Project Structure

```
vehicle_gps_tracker/
├── include/
│   ├── gps.h                    # GPS module interface
│   ├── max6675.h               # Temperature sensor interface
│   ├── mpu6050.h               # IMU interface
│   ├── vehicle_performance.h   # Performance calculator
│   ├── web_config.h            # Web configuration interface
│   ├── wifi.h                  # WiFi manager interface
│   └── mqtt_vehicle_client.h   # MQTT client interface
├── src/
│   ├── main.c                  # Main application logic
│   ├── gps.c                   # GPS NMEA parser
│   ├── max6675.c               # MAX6675 SPI driver
│   ├── mpu6050.c               # MPU6050 I2C driver
│   ├── vehicle_performance.c   # Performance tracking logic
│   ├── web_config.c            # Web server for configuration
│   ├── wifi_manager.c          # WiFi connection manager
│   ├── mqtt_vehicle_client.c   # MQTT communication handler
│   └── CMakeLists.txt
├── platformio.ini              # PlatformIO configuration
├── sdkconfig.esp32doit-devkit-v1  # ESP-IDF SDK configuration
├── CMakeLists.txt              # Root CMake configuration
└── README.md                   # This file
```

## 🐛 Troubleshooting

### GPS Not Getting Fix
- Ensure GPS module has clear view of sky
- Check UART connections (TX/RX not swapped)
- Verify baud rate is 9600
- Wait 30-60 seconds for initial fix (cold start)

### Cannot Connect to WiFi
- Verify SSID and password in `main.c`
- Check WiFi network is 2.4GHz (ESP32 doesn't support 5GHz)
- Ensure WiFi network is in range

### MQTT Connection Issues
- Verify RabbitMQ broker is accessible
- Check MQTT credentials in `mqtt_vehicle_client.h`
- Ensure port 1883 is open on broker

### Sensor Initialization Failed
- Check I2C/SPI connections
- Verify pull-up resistors on I2C lines
- Test sensors individually using example code

## 📝 Configuration Files

### RabbitMQ Configuration
See `rabbitmq.conf` for complete broker setup including:
- MQTT plugin configuration
- Queue policies
- User permissions
- Exchange bindings

### ESP-IDF SDK Config
The `sdkconfig.esp32doit-devkit-v1` contains:
- WiFi configuration
- MQTT settings
- Memory optimization
- Debug settings

## 🔐 Security Notes

- Change default MQTT credentials in production
- Use TLS/SSL for MQTT in production environment
- Implement authentication for web configuration interface
- Store sensitive data in encrypted NVS partition

## 📊 Performance Metrics

- **RAM Usage**: ~100KB
- **Flash Usage**: ~1.2MB
- **Update Frequency**:
  - GPS: 5 seconds
  - Status: 5 seconds
  - Battery: 10 seconds
  - Temperature: 5 seconds

## 🤝 Contributing

1. Fork the repository
2. Create feature branch (`git checkout -b feature/AmazingFeature`)
3. Commit changes (`git commit -m 'Add AmazingFeature'`)
4. Push to branch (`git push origin feature/AmazingFeature`)
5. Open Pull Request

## 📜 License

This project is licensed under the MIT License - see LICENSE file for details.

## 👥 Authors

- C7 Apple Developer Academy UC

## 🙏 Acknowledgments

- ESP-IDF Framework
- RabbitMQ Team
- PlatformIO Community