import asyncio
import websockets
import json
import cv2
import numpy as np
import base64
import serial
import serial.tools.list_ports
import os
import glob
import time
import http.server
import pytesseract
import socketserver
import threading
import sys
import webbrowser
import subprocess
import atexit

PORT_HTTP = 8080
PORT_WS = 8081
TEMPLATE_DIR = "templates"
CONFIDENCE_THRESHOLD = 0.85

text_macros = []
serial_conn = None
last_triggered_key = None
debounce_time = 0

def load_text_macros():
    global text_macros
    try:
        with open("text_macros.json", "r") as f:
            text_macros = json.load(f)
        print(f"Loaded {len(text_macros)} text macro(s) for OCR.")
    except Exception as e:
        print("Failed to load text_macros.json:", e)
        text_macros = []

def serve_http():
    web_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'web')
    
    class Handler(http.server.SimpleHTTPRequestHandler):
        def __init__(self, *args, **kwargs):
            super().__init__(*args, directory=web_dir, **kwargs)
            
    httpd = http.server.ThreadingHTTPServer(("", PORT_HTTP), Handler)
    print(f"HTTP Server running at http://localhost:{PORT_HTTP}")
    httpd.serve_forever()

async def ws_handler(websocket):
    global serial_conn, last_triggered_key, debounce_time
    
    try:
        async for message in websocket:
            data = json.loads(message)
            
            if data["type"] == "get_ports":
                is_connected = serial_conn is not None and serial_conn.is_open
                await websocket.send(json.dumps({"type": "status", "esp_connected": is_connected}))
                
                ports = [port.device for port in serial.tools.list_ports.comports()]
                await websocket.send(json.dumps({"type": "ports", "ports": ports}))
                
            elif data["type"] == "connect_serial":
                try:
                    if serial_conn:
                        serial_conn.close()
                    serial_conn = serial.Serial(data["port"], 115200, timeout=1)
                    await websocket.send(json.dumps({"type": "log", "msg": f"Connected to ESP32 on {data['port']}"}))
                    await websocket.send(json.dumps({"type": "status", "esp_connected": True}))
                except Exception as e:
                    await websocket.send(json.dumps({"type": "log", "msg": f"Connection error: {str(e)}"}))
                    await websocket.send(json.dumps({"type": "status", "esp_connected": False}))
                    
            elif data["type"] == "trigger":
                if serial_conn and serial_conn.is_open:
                    cmd = data["key"].encode() + b'\n'
                    serial_conn.write(cmd)
                    await websocket.send(json.dumps({"type": "log", "msg": f"Sent physical command: '{data['key']}'"}))
                else:
                    await websocket.send(json.dumps({"type": "log", "msg": "Cannot send command. ESP32 not connected!"}))
                    
            elif data["type"] == "stop_autopilot":
                last_triggered_key = None
                await websocket.send(json.dumps({"type": "log", "msg": "Backend acknowledged stop command."}))
                
            elif data["type"] == "frame":
                current_time = time.time()
                
                # Decode Base64 JPEG
                header, encoded = data["image"].split(",", 1)
                img_bytes = base64.b64decode(encoded)
                np_arr = np.frombuffer(img_bytes, np.uint8)
                frame = cv2.imdecode(np_arr, cv2.IMREAD_COLOR)
                
                if frame is None:
                    continue
                    
                # Convert to grayscale to improve OCR
                gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
                ocr_data = pytesseract.image_to_data(gray, output_type=pytesseract.Output.DICT)
                
                matches = []
                n_boxes = len(ocr_data['text'])
                for i in range(n_boxes):
                    word = ocr_data['text'][i].strip()
                    if len(word) < 3:
                        continue
                        
                    for macro in text_macros:
                        target = macro["text"]
                        target_words = [w.lower() for w in target.split()]
                        is_match = False
                        for tw in target_words:
                            if tw in word.lower():
                                is_match = True
                                break
                                
                        if is_match:
                            if int(ocr_data['conf'][i]) > 60:
                                matches.append({
                                    "name": macro["name"],
                                    "key": macro["key"]
                                })
                                
                if matches:
                    best_match = matches[0]
                    trigger_key = best_match["key"]
                    
                    # A macro will ONLY trigger exactly once. It will NEVER repeat unless a different macro triggers first.
                    if trigger_key != last_triggered_key and (current_time - debounce_time > 2.0):
                        if serial_conn and serial_conn.is_open:
                            serial_conn.write(trigger_key.encode() + b'\n')
                            await websocket.send(json.dumps({"type": "log", "msg": f"OCR MATCH: {best_match['name']} -> Triggered {trigger_key}"}))
                            await websocket.send(json.dumps({"type": "trigger_fired", "key": trigger_key}))
                        
                        last_triggered_key = trigger_key
                        debounce_time = current_time
                    
                    await websocket.send(json.dumps({"type": "match", "matches": [best_match]}))
                
                await websocket.send(json.dumps({"type": "frame_processed"}))
                
    except websockets.exceptions.ConnectionClosed:
        pass

async def main():
    global serial_conn
    load_text_macros()
    
    # Auto-Connect to ESP32 silently in the background
    print("Searching for ESP32 on USB...")
    ports = [p.device for p in serial.tools.list_ports.comports()]
    for p in ports:
        if 'USB' in p or 'ACM' in p:
            try:
                serial_conn = serial.Serial(p, 115200, timeout=1)
                print(f"Successfully auto-connected to ESP32 on {p}!")
                break
            except Exception as e:
                print(f"Failed to auto-connect to {p}: {e}")
    
    # Start HTTP server in a separate thread
    http_thread = threading.Thread(target=serve_http, daemon=True)
    http_thread.start()
    
    # Open the UI in Chrome/Firefox
    time.sleep(0.5)
    webbrowser.open(f"http://localhost:{PORT_HTTP}")
    
    # Start WebSocket server
    print(f"WebSocket Server running on ws://localhost:{PORT_WS}")
    async with websockets.serve(ws_handler, "localhost", PORT_WS):
        await asyncio.Future()  # run forever

if __name__ == "__main__":
    asyncio.run(main())
