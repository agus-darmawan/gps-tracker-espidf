#!/usr/bin/env python3
"""
Real-time Vehicle Monitor
Subscribe to vehicle data streams and display in console
"""

import pika
import json
import sys
from datetime import datetime

RABBITMQ_HOST = "103.175.219.138"
RABBITMQ_PORT = 5672
RABBITMQ_USER = "backend"
RABBITMQ_PASS = "backend123"
EXCHANGE = "vehicle.exchange"

class VehicleMonitor:
    def __init__(self, vehicle_id):
        self.vehicle_id = vehicle_id
        self.connection = None
        self.channel = None
        
        # Statistics
        self.location_count = 0
        self.status_count = 0
        self.battery_count = 0
        self.performance_count = 0
        
        self.last_location = None
        self.last_status = None
        self.last_battery = None
    
    def connect(self):
        """Connect to RabbitMQ"""
        try:
            credentials = pika.PlainCredentials(RABBITMQ_USER, RABBITMQ_PASS)
            parameters = pika.ConnectionParameters(
                host=RABBITMQ_HOST,
                port=RABBITMQ_PORT,
                credentials=credentials,
                heartbeat=60
            )
            self.connection = pika.BlockingConnection(parameters)
            self.channel = self.connection.channel()
            print(f"✅ Connected to RabbitMQ at {RABBITMQ_HOST}")
        except Exception as e:
            print(f"❌ Failed to connect to RabbitMQ: {e}")
            sys.exit(1)
    
    def setup_queues(self):
        """Setup queues and bindings"""
        try:
            # Declare queues
            queues = [
                'vehicle.realtime.location',
                'vehicle.realtime.status',
                'vehicle.realtime.battery',
                'vehicle.report.performance'
            ]
            
            for queue in queues:
                self.channel.queue_declare(queue=queue, durable=True, passive=True)
            
            # Bind to specific vehicle
            self.channel.queue_bind(
                exchange=EXCHANGE,
                queue='vehicle.realtime.location',
                routing_key=f'realtime.location.{self.vehicle_id}'
            )
            
            self.channel.queue_bind(
                exchange=EXCHANGE,
                queue='vehicle.realtime.status',
                routing_key=f'realtime.status.{self.vehicle_id}'
            )
            
            self.channel.queue_bind(
                exchange=EXCHANGE,
                queue='vehicle.realtime.battery',
                routing_key=f'realtime.battery.{self.vehicle_id}'
            )
            
            self.channel.queue_bind(
                exchange=EXCHANGE,
                queue='vehicle.report.performance',
                routing_key=f'report.performance.{self.vehicle_id}'
            )
            
            print(f"✅ Queues configured for vehicle: {self.vehicle_id}\n")
            
        except Exception as e:
            print(f"❌ Failed to setup queues: {e}")
            sys.exit(1)
    
    def handle_location(self, ch, method, properties, body):
        """Handle location messages"""
        try:
            data = json.loads(body)
            self.location_count += 1
            self.last_location = data
            
            lat = data.get('latitude', 0)
            lon = data.get('longitude', 0)
            alt = data.get('altitude', 0)
            timestamp = data.get('timestamp', '')
            
            print(f"📍 LOCATION UPDATE #{self.location_count}")
            print(f"   Coordinates: {lat:.6f}, {lon:.6f}")
            print(f"   Altitude: {alt:.2f}m")
            print(f"   Time: {timestamp}")
            print()
            
            ch.basic_ack(delivery_tag=method.delivery_tag)
        except Exception as e:
            print(f"❌ Error processing location: {e}")
            ch.basic_nack(delivery_tag=method.delivery_tag)
    
    def handle_status(self, ch, method, properties, body):
        """Handle status messages"""
        try:
            data = json.loads(body)
            self.status_count += 1
            self.last_status = data
            
            active = data.get('is_active', False)
            locked = data.get('is_locked', True)
            killed = data.get('is_killed', False)
            timestamp = data.get('timestamp', '')
            
            status_emoji = "🟢" if active else "🔴"
            lock_emoji = "🔒" if locked else "🔓"
            kill_emoji = "💀" if killed else "✅"
            
            print(f"{status_emoji} STATUS UPDATE #{self.status_count}")
            print(f"   Active: {active} | Locked: {locked} {lock_emoji} | Killed: {killed} {kill_emoji}")
            print(f"   Time: {timestamp}")
            print()
            
            ch.basic_ack(delivery_tag=method.delivery_tag)
        except Exception as e:
            print(f"❌ Error processing status: {e}")
            ch.basic_nack(delivery_tag=method.delivery_tag)
    
    def handle_battery(self, ch, method, properties, body):
        """Handle battery messages"""
        try:
            data = json.loads(body)
            self.battery_count += 1
            self.last_battery = data
            
            voltage = data.get('device_voltage', 0)
            level = data.get('device_battery_level', 0)
            timestamp = data.get('timestamp', '')
            
            battery_emoji = "🔋" if level > 50 else "🪫"
            
            print(f"{battery_emoji} BATTERY UPDATE #{self.battery_count}")
            print(f"   Voltage: {voltage:.2f}V | Level: {level:.2f}%")
            print(f"   Time: {timestamp}")
            print()
            
            ch.basic_ack(delivery_tag=method.delivery_tag)
        except Exception as e:
            print(f"❌ Error processing battery: {e}")
            ch.basic_nack(delivery_tag=method.delivery_tag)
    
    def handle_performance(self, ch, method, properties, body):
        """Handle performance report"""
        try:
            data = json.loads(body)
            self.performance_count += 1
            
            order_id = data.get('order_id', 'N/A')
            weight = data.get('weight_score', 'N/A')
            distance = data.get('distance_travelled', 0)
            avg_speed = data.get('average_speed', 0)
            max_speed = data.get('max_speed', 0)
            
            print("="*60)
            print(f"📊 PERFORMANCE REPORT #{self.performance_count}")
            print("="*60)
            print(f"Order ID: {order_id}")
            print(f"Weight Score: {weight}")
            print(f"Distance: {distance:.2f} km")
            print(f"Avg Speed: {avg_speed:.2f} km/h")
            print(f"Max Speed: {max_speed:.2f} km/h")
            print("\nComponent Wear:")
            print(f"  Front Tire: {data.get('front_tire', 0):,}m")
            print(f"  Rear Tire: {data.get('rear_tire', 0):,}m")
            print(f"  Brake Pad: {data.get('brake_pad', 0):,}m")
            print(f"  Engine Oil: {data.get('engine_oil', 0):,}m")
            print(f"  Chain/CVT: {data.get('chain_or_cvt', 0):,}m")
            print(f"  Engine (Total): {data.get('engine', 0):,}m")
            print("="*60 + "\n")
            
            ch.basic_ack(delivery_tag=method.delivery_tag)
        except Exception as e:
            print(f"❌ Error processing performance: {e}")
            ch.basic_nack(delivery_tag=method.delivery_tag)
    
    def start_monitoring(self):
        """Start consuming messages"""
        print("="*60)
        print(f"🚗 Monitoring Vehicle: {self.vehicle_id}")
        print("="*60)
        print("Listening for messages... (Press Ctrl+C to stop)\n")
        
        # Setup consumers
        self.channel.basic_consume(
            queue='vehicle.realtime.location',
            on_message_callback=self.handle_location,
            auto_ack=False
        )
        
        self.channel.basic_consume(
            queue='vehicle.realtime.status',
            on_message_callback=self.handle_status,
            auto_ack=False
        )
        
        self.channel.basic_consume(
            queue='vehicle.realtime.battery',
            on_message_callback=self.handle_battery,
            auto_ack=False
        )
        
        self.channel.basic_consume(
            queue='vehicle.report.performance',
            on_message_callback=self.handle_performance,
            auto_ack=False
        )
        
        try:
            self.channel.start_consuming()
        except KeyboardInterrupt:
            print("\n\n🛑 Stopping monitor...")
            self.print_summary()
            self.channel.stop_consuming()
            self.connection.close()
    
    def print_summary(self):
        """Print monitoring summary"""
        print("\n" + "="*60)
        print("📈 MONITORING SUMMARY")
        print("="*60)
        print(f"Vehicle ID: {self.vehicle_id}")
        print(f"Location updates: {self.location_count}")
        print(f"Status updates: {self.status_count}")
        print(f"Battery updates: {self.battery_count}")
        print(f"Performance reports: {self.performance_count}")
        
        if self.last_location:
            print(f"\nLast known position:")
            print(f"  {self.last_location.get('latitude', 0):.6f}, {self.last_location.get('longitude', 0):.6f}")
        
        if self.last_status:
            print(f"\nLast status:")
            print(f"  Active: {self.last_status.get('is_active', False)}")
            print(f"  Locked: {self.last_status.get('is_locked', True)}")
        
        if self.last_battery:
            print(f"\nLast battery:")
            print(f"  {self.last_battery.get('device_battery_level', 0):.2f}% ({self.last_battery.get('device_voltage', 0):.2f}V)")
        
        print("="*60 + "\n")

def main():
    if len(sys.argv) < 2:
        print("""
╔═══════════════════════════════════════════════════════════════╗
║          Vehicle GPS Tracker - Real-time Monitor              ║
╚═══════════════════════════════════════════════════════════════╝

Usage:
    python monitor_vehicle.py <vehicle_id>

Example:
    python monitor_vehicle.py B1234ABC

This will monitor all real-time data streams from the specified vehicle.
        """)
        sys.exit(1)
    
    vehicle_id = sys.argv[1]
    
    monitor = VehicleMonitor(vehicle_id)
    monitor.connect()
    monitor.setup_queues()
    monitor.start_monitoring()

if __name__ == "__main__":
    main()