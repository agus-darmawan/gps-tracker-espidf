# Installation Guide - Vehicle GPS Tracker

## 📋 Prerequisites

### Hardware
- ESP32 development board (ESP32-DOIT-DEVKIT-V1 recommended)
- USB cable (data capable)
- GPS Module (UART interface)
- MAX6675 thermocouple module (SPI interface)
- MPU6050 IMU module (I2C interface)
- Breadboard and jumper wires

### Software
- Python 3.8+ (for testing scripts)
- PlatformIO IDE or CLI
- Git (for cloning repository)

## 🔌 Hardware Wiring

### GPS Module (UART)
```
GPS Module    →    ESP32
VCC           →    3.3V
GND           →    GND
TX            →    GPIO16 (RX2)
RX            →    GPIO17 (TX2)
```

### MAX6675 Thermocouple (SPI)
```
MAX6675       →    ESP32
VCC           →    3.3V or 5V
GND           →    GND
SCK           →    GPIO18 (SCK)
SO (MISO)     →    GPIO19 (MISO)
CS            →    GPIO5
```

### MPU6050 IMU (I2C)
```
MPU6050       →    ESP32
VCC           →    3.3V
GND           →    GND
SCL           →    GPIO22 (SCL)
SDA           →    GPIO21 (SDA)
```

### Pin Summary
| Component | Pin | GPIO | Function |
|-----------|-----|------|----------|
| GPS TX | RX2 | GPIO16 | UART Receive |
| GPS RX | TX2 | GPIO17 | UART Transmit |
| MAX6675 SCK | SCK | GPIO18 | SPI Clock |
| MAX6675 SO | MISO | GPIO19 | SPI Data In |
| MAX6675 CS | CS | GPIO5 | Chip Select |
| MPU6050 SCL | SCL | GPIO22 | I2C Clock |
| MPU6050 SDA | SDA | GPIO21 | I2C Data |

## 💻 Software Installation

### 1. Install PlatformIO

#### Option A: VS Code Extension
1. Install [Visual Studio Code](https://code.visualstudio.com/)
2. Install PlatformIO IDE extension from VS Code marketplace
3. Restart VS Code

#### Option B: CLI Installation
```bash
# Install via pip
pip install -U platformio

# Or via script (Linux/Mac)
curl -fsSL https://raw.githubusercontent.com/platformio/platformio-core/master/scripts/get-platformio.py | python3
```

### 2. Clone/Download Project

```bash
# Clone from repository (if available)
git clone <repository-url>
cd vehicle_gps_tracker

# Or extract from zip file
unzip vehicle_gps_tracker.zip
cd vehicle_gps_tracker
```

### 3. Configure WiFi Credentials

Edit `src/main.c` and update WiFi credentials:

```c
// Around line 16-17
#define WIFI_SSID "your_wifi_ssid"
#define WIFI_PASS "your_wifi_password"
```

**Important**: Use 2.4GHz WiFi network. ESP32 does not support 5GHz.

### 4. Configure MQTT Broker (Optional)

If you're using a different RabbitMQ broker, edit `include/mqtt_vehicle_client.h`:

```c
// Line 8-10
#define MQTT_BROKER_URI     "mqtt://your-broker-ip:1883"
#define MQTT_USERNAME       "vehicle"
#define MQTT_PASSWORD       "vehicle123"
```

## 🚀 Build and Upload

### Using PlatformIO IDE (VS Code)

1. Open project folder in VS Code
2. Wait for PlatformIO to initialize
3. Connect ESP32 via USB
4. Click "Upload" button in status bar (→ icon)
5. Wait for compilation and upload
6. Open Serial Monitor (🔌 icon) to see output

### Using PlatformIO CLI

```bash
# Navigate to project directory
cd vehicle_gps_tracker

# Build project
pio run

# Upload to ESP32
pio run --target upload

# Monitor serial output
pio run --target monitor

# All in one command
pio run --target upload --target monitor
```

### Expected Serial Output

```
[MAIN] Vehicle GPS Tracker Starting...
[WIFI_MANAGER] WiFi init finished. Connecting to YourSSID
[WIFI_MANAGER] Connected to WiFi with IP: 192.168.1.100
[GPS] GPS initialized on UART2 (TX: GPIO17, RX: GPIO16)
[MAX6675] MAX6675 initialized successfully
[MPU6050] MPU6050 initialized successfully
[PERFORMANCE] Performance tracking initialized
[MQTT_VEHICLE] MQTT client initialized for vehicle: B1234ABC
[MAIN] System initialized successfully!
```

## ⚙️ Initial Configuration

### 1. Find ESP32 IP Address

Check serial monitor output for IP address:
```
[WIFI_MANAGER] Connected to WiFi with IP: 192.168.1.100
```

### 2. Access Web Configuration

1. Open browser and navigate to: `http://192.168.1.100`
2. Enter Vehicle ID (plate number, e.g., "B1234ABC")
3. Click "Save Configuration"
4. ESP32 will restart automatically

### 3. Verify Configuration

After restart, check serial monitor:
```
[MAIN] Vehicle ID: B1234ABC
[MQTT_VEHICLE] Connected to MQTT broker
[MQTT_VEHICLE] Published registration for vehicle: B1234ABC
```

## 🧪 Testing

### Install Python Dependencies

```bash
cd vehicle_gps_tracker
pip install -r requirements.txt
```

### Monitor Real-time Data

```bash
# Monitor all data streams from vehicle
python monitor_vehicle.py B1234ABC
```

Expected output:
```
✅ Connected to RabbitMQ at 103.175.219.138
✅ Queues configured for vehicle: B1234ABC

🚗 Monitoring Vehicle: B1234ABC
================================================================
Listening for messages... (Press Ctrl+C to stop)

📍 LOCATION UPDATE #1
   Coordinates: -6.208800, 106.845600
   Altitude: 10.50m
   Time: 2025-11-12T10:30:00.000Z

🟢 STATUS UPDATE #1
   Active: False | Locked: True 🔒 | Killed: False ✅
   Time: 2025-11-12T10:30:05.000Z
```

### Send Test Commands

```bash
# Start rental
python test_commands.py B1234ABC start_rent

# End rental
python test_commands.py B1234ABC end_rent

# Emergency kill
python test_commands.py B1234ABC kill
```

## 🔧 Troubleshooting

### ESP32 Not Connecting to WiFi

**Problem**: Serial shows "Waiting for WiFi connection..."

**Solutions**:
1. Verify SSID and password are correct
2. Ensure WiFi is 2.4GHz (not 5GHz)
3. Check WiFi signal strength
4. Try restarting router
5. Disable MAC address filtering temporarily

### GPS Not Getting Fix

**Problem**: Log shows "Waiting for GPS fix..."

**Solutions**:
1. Move device near window or outdoors
2. Wait 30-60 seconds for cold start
3. Check GPS antenna connection
4. Verify UART wiring (TX/RX not swapped)
5. Test GPS module separately

### Sensor Initialization Failed

**Problem**: "MAX6675 initialization failed" or "MPU6050 initialization failed"

**Solutions**:
1. Check power connections (3.3V, GND)
2. Verify I2C/SPI wiring
3. Test with I2C/SPI scanner sketch
4. Check for loose connections
5. Try different GPIO pins if conflicts exist

### MQTT Connection Issues

**Problem**: Cannot connect to MQTT broker

**Solutions**:
1. Verify broker IP address and port
2. Check network connectivity: `ping 103.175.219.138`
3. Ensure port 1883 is not blocked by firewall
4. Verify MQTT credentials
5. Check RabbitMQ MQTT plugin is enabled

### Python Scripts Not Working

**Problem**: ImportError or connection errors

**Solutions**:
```bash
# Reinstall dependencies
pip install --upgrade pika

# Check Python version
python --version  # Should be 3.8+

# Test RabbitMQ connection
python -c "import pika; print('Pika installed correctly')"
```

### Serial Monitor Shows Garbage Characters

**Problem**: Unreadable characters in serial output

**Solutions**:
1. Set baud rate to 115200
2. Close other serial monitor instances
3. Try different USB cable
4. Reinstall USB drivers
5. Check for hardware issues with ESP32

## 📊 Performance Tuning

### Reduce Memory Usage

If experiencing memory issues, adjust these in `src/main.c`:

```c
// Reduce task stack sizes
xTaskCreate(gps_task, "gps_task", 2048, NULL, 5, NULL);  // was 4096
xTaskCreate(vehicle_tracking_task, "vehicle_tracking", 4096, NULL, 5, NULL);  // was 8192
```

### Adjust Update Intervals

Change update frequencies in `src/main.c`:

```c
#define GPS_UPDATE_INTERVAL     10000   // 10 seconds (was 5)
#define STATUS_UPDATE_INTERVAL  10000   // 10 seconds (was 5)
#define BATTERY_UPDATE_INTERVAL 20000   // 20 seconds (was 10)
```

### Optimize Log Level

Reduce log verbosity in `platformio.ini`:

```ini
build_flags = 
    -DCORE_DEBUG_LEVEL=2  ; 0=None, 1=Error, 2=Warn, 3=Info, 4=Debug, 5=Verbose
```

## 🔒 Security Best Practices

### Production Deployment

1. **Change Default Credentials**:
   ```c
   // In mqtt_vehicle_client.h
   #define MQTT_USERNAME "vehicle_prod_001"
   #define MQTT_PASSWORD "str0ng_p@ssw0rd_here"
   ```

2. **Enable TLS/SSL**:
   - Configure RabbitMQ with SSL certificates
   - Update broker URI to `mqtts://` instead of `mqtt://`

3. **Implement Web Server Authentication**:
   - Add password protection to web configuration interface
   - Use HTTPS instead of HTTP

4. **Secure NVS Storage**:
   - Enable NVS encryption in sdkconfig
   - Store sensitive data in encrypted partition

## 📞 Support

For issues and questions:
- Check [README.md](README.md) for system overview
- Review [FLOW_DIAGRAM.md](FLOW_DIAGRAM.md) for system architecture
- Open an issue on GitHub (if applicable)
- Contact technical support team

## 📝 Next Steps

After successful installation:

1. Test all sensors individually
2. Verify MQTT communication
3. Perform test rental cycle (start_rent → end_rent)
4. Review performance reports
5. Set up production monitoring dashboard
6. Configure automated alerts
7. Deploy to vehicles

## ✅ Installation Checklist

- [ ] Hardware wired correctly
- [ ] PlatformIO installed
- [ ] Project downloaded/cloned
- [ ] WiFi credentials configured
- [ ] MQTT broker settings verified
- [ ] Firmware uploaded successfully
- [ ] Serial output shows successful boot
- [ ] Vehicle ID configured via web interface
- [ ] GPS getting valid fix
- [ ] MQTT connection established
- [ ] Python test scripts working
- [ ] Real-time monitoring verified
- [ ] Test commands executed successfully
- [ ] Performance report generated correctly

**Congratulations! Your Vehicle GPS Tracker is ready to use! 🎉**