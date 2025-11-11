#!/usr/bin/env python3
"""
MQTT Command Tester for Vehicle GPS Tracker
Send test commands to ESP32 device
"""

import pika
import json
import sys
import time
from datetime import datetime, UTC

RABBITMQ_HOST = "103.175.219.138"
RABBITMQ_PORT = 5672
RABBITMQ_USER = "backend"
RABBITMQ_PASS = "backend123"
EXCHANGE = "vehicle.exchange"

def get_timestamp():
    """Get current ISO8601 timestamp"""
    return datetime.now(UTC).isoformat()

def send_command(vehicle_id, command, payload=None):
    """
    Send command to vehicle via RabbitMQ
    
    Args:
        vehicle_id: Vehicle ID (e.g., "B1234ABC")
        command: Command type ("start_rent", "end_rent", "kill_vehicle")
        payload: Optional JSON payload dictionary
    """
    try:
        # Connect to RabbitMQ
        credentials = pika.PlainCredentials(RABBITMQ_USER, RABBITMQ_PASS)
        parameters = pika.ConnectionParameters(
            host=RABBITMQ_HOST,
            port=RABBITMQ_PORT,
            credentials=credentials
        )
        connection = pika.BlockingConnection(parameters)
        channel = connection.channel()
        
        # Prepare routing key
        routing_key = f"control.{command}.{vehicle_id}"
        
        # Prepare payload
        if payload is None:
            payload = {}
        
        message = json.dumps(payload)
        
        # Publish command
        channel.basic_publish(
            exchange=EXCHANGE,
            routing_key=routing_key,
            body=message
        )
        
        print(f"✅ Command sent successfully:")
        print(f"   Vehicle: {vehicle_id}")
        print(f"   Command: {command}")
        print(f"   Payload: {message}")
        
        connection.close()
        
    except Exception as e:
        print(f"❌ Error sending command: {e}")
        sys.exit(1)

def start_rent(vehicle_id, order_id=None):
    """Send start_rent command"""
    if order_id is None:
        order_id = f"ORD-{vehicle_id}-{int(time.time())}"
    
    payload = {
        "order_id": order_id,
        "timestamp": get_timestamp()
    }
    
    send_command(vehicle_id, "start_rent", payload)
    print(f"\n📝 Order ID: {order_id}")
    print("🔓 Vehicle should now be unlocked and tracking performance\n")

def end_rent(vehicle_id):
    """Send end_rent command"""
    payload = {
        "timestamp": get_timestamp()
    }
    
    send_command(vehicle_id, "end_rent", payload)
    print("\n🔒 Vehicle should now be locked")
    print("📊 Performance report should be published\n")

def kill_vehicle(vehicle_id):
    """Send kill_vehicle command"""
    payload = {
        "timestamp": get_timestamp()
    }
    
    send_command(vehicle_id, "kill_vehicle", payload)
    print("\n⚠️  Kill command scheduled")
    print("🛑 Vehicle will be killed when speed < 10 km/h\n")

def print_usage():
    """Print usage instructions"""
    print("""
╔═══════════════════════════════════════════════════════════════╗
║          Vehicle GPS Tracker - MQTT Command Tester            ║
╚═══════════════════════════════════════════════════════════════╝

Usage:
    python test_commands.py <vehicle_id> <command> [order_id]

Commands:
    start_rent [order_id]  - Start rental and begin tracking
    end_rent               - End rental and generate report
    kill                   - Emergency vehicle shutdown

Examples:
    python test_commands.py B1234ABC start_rent
    python test_commands.py B1234ABC start_rent ORD-123456
    python test_commands.py B1234ABC end_rent
    python test_commands.py B1234ABC kill

Vehicle ID Format:
    - Indonesian plate number format (e.g., B1234ABC)
    - Must match the vehicle_id configured on ESP32
    """)

def main():
    if len(sys.argv) < 3:
        print_usage()
        sys.exit(1)
    
    vehicle_id = sys.argv[1]
    command = sys.argv[2].lower()
    
    print("\n" + "="*60)
    print(f"Vehicle GPS Tracker - Command Test")
    print("="*60)
    print(f"Target Vehicle: {vehicle_id}")
    print(f"Command: {command}")
    print(f"Time: {get_timestamp()}")
    print("="*60 + "\n")
    
    if command == "start_rent" or command == "start":
        order_id = sys.argv[3] if len(sys.argv) > 3 else None
        start_rent(vehicle_id, order_id)
        
    elif command == "end_rent" or command == "end":
        end_rent(vehicle_id)
        
    elif command == "kill_vehicle" or command == "kill":
        kill_vehicle(vehicle_id)
        
    else:
        print(f"❌ Unknown command: {command}")
        print_usage()
        sys.exit(1)
    
    print("="*60)
    print("✅ Test completed successfully!")
    print("="*60 + "\n")

if __name__ == "__main__":
    main()