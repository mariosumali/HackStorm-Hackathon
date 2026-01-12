import serial
import serial.tools.list_ports
import time
import sys

def list_ports():
    ports = serial.tools.list_ports.comports()
    return [p.device for p in ports]

def test_connection():
    target_port = None
    
    # Check args
    if len(sys.argv) > 1:
        target_port = sys.argv[1]
    
    if not target_port:
        ports = list_ports()
        print(f"Available ports: {ports}")
        # Auto-select likely port
        for p in ports:
            if "usbmodem" in p and "871" in p: # Prefer 871 based on history
                target_port = p
                break
        if not target_port and ports:
            target_port = ports[0]
            
    if not target_port:
        print("No ports found!")
        return

    print(f"Attempting to connect to {target_port}...")
    
    try:
        ser = serial.Serial(target_port, baudrate=921600, timeout=1)
        print(f"Successfully opened {target_port}")
        print("Listening for data... (Press Ctrl+C to stop)")
        print("-" * 40)
        
        start_time = time.time()
        byte_count = 0
        
        while True:
            if ser.in_waiting:
                try:
                    line = ser.readline().decode('utf-8', errors='replace').strip()
                    if line:
                        print(f"[DEVICE]: {line}")
                        byte_count += len(line)
                        start_time = time.time() # Reset timeout on data
                except Exception as e:
                    print(f"Error reading: {e}")
            
            # Timeout message every 10s if silence
            if time.time() - start_time > 10 and byte_count == 0:
                print(".", end="", flush=True)
                start_time = time.time() 
                
    except serial.SerialException as e:
        print(f"Failed to connect: {e}")
    except KeyboardInterrupt:
        print("\nTest stopped by user.")

if __name__ == "__main__":
    test_connection()
