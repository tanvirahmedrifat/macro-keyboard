import customtkinter as ctk
import cv2
import numpy as np
from PIL import Image, ImageTk
import pytesseract
import serial
import serial.tools.list_ports
import json
import threading
import time
import os

ctk.set_appearance_mode("Dark")
ctk.set_default_color_theme("blue")

class NativeAutomator(ctk.CTk):
    def __init__(self):
        super().__init__()

        self.title("Macro Keyboard - Native Auto-Pilot")
        self.geometry("1100x750")
        
        self.serial_conn = None
        self.text_macros = []
        self.auto_pilot_running = False
        self.cap = None
        self.last_triggered_key = None
        self.debounce_time = 0
        self.macro_buttons = {}
        self.last_detected_words = []
        self.last_match_text = ""
        self.last_match_time = 0
        
        self.serial_thread = None
        self.keep_reading_serial = False
        
        # Cumulative mouse tracking
        self.total_dx = 0
        self.total_dy = 0
        self.is_dragging = False
        
        self.build_ui()
        self.load_text_macros()
        self.refresh_ports()
        
        # Start camera loop
        self.start_camera()

    def load_text_macros(self):
        try:
            with open("text_macros.json", "r") as f:
                self.text_macros = json.load(f)
            self.log(f"Loaded {len(self.text_macros)} text macro(s) for OCR.")
        except Exception as e:
            self.log(f"Failed to load text_macros.json: {e}")
            self.text_macros = []
            
        self.rebuild_virtual_pad()

    def save_text_macros(self):
        try:
            with open("text_macros.json", "w") as f:
                json.dump(self.text_macros, f, indent=4)
            self.log("Saved macro settings to text_macros.json")
        except Exception as e:
            self.log(f"Failed to save text_macros.json: {e}")

    def build_ui(self):
        # Configure grid
        self.grid_columnconfigure(1, weight=1)
        self.grid_rowconfigure(0, weight=1)
        
        # ── LEFT SIDEBAR ──
        self.sidebar_frame = ctk.CTkFrame(self, width=320, corner_radius=0)
        self.sidebar_frame.grid(row=0, column=0, sticky="nsew")
        self.sidebar_frame.grid_rowconfigure(4, weight=1)
        
        self.logo_label = ctk.CTkLabel(self.sidebar_frame, text="Auto-Pilot Dashboard", font=ctk.CTkFont(size=20, weight="bold"))
        self.logo_label.grid(row=0, column=0, padx=20, pady=(20, 10))
        
        # Connection
        self.conn_frame = ctk.CTkFrame(self.sidebar_frame)
        self.conn_frame.grid(row=1, column=0, padx=20, pady=10, sticky="ew")
        
        self.port_var = ctk.StringVar(value="Select Port")
        self.port_cb = ctk.CTkComboBox(self.conn_frame, variable=self.port_var, values=[])
        self.port_cb.pack(padx=10, pady=(10, 5), fill="x")
        
        btn_frame = ctk.CTkFrame(self.conn_frame, fg_color="transparent")
        btn_frame.pack(padx=10, pady=(0, 10), fill="x")
        
        self.btn_refresh = ctk.CTkButton(btn_frame, text="Refresh", width=80, command=self.refresh_ports)
        self.btn_refresh.pack(side="left", padx=(0, 5), expand=True, fill="x")
        
        self.btn_connect = ctk.CTkButton(btn_frame, text="Connect", width=80, command=self.toggle_connection)
        self.btn_connect.pack(side="right", padx=(5, 0), expand=True, fill="x")
        
        # Virtual Pad
        self.pad_frame = ctk.CTkFrame(self.sidebar_frame)
        self.pad_frame.grid(row=2, column=0, padx=20, pady=10, sticky="ew")
        ctk.CTkLabel(self.pad_frame, text="Virtual Macro Pad", font=ctk.CTkFont(weight="bold")).pack(pady=(5, 0))
        
        self.pad_list_frame = ctk.CTkFrame(self.pad_frame, fg_color="transparent")
        self.pad_list_frame.pack(padx=10, pady=10, fill="x")
                
        # Auto Pilot
        self.auto_frame = ctk.CTkFrame(self.sidebar_frame)
        self.auto_frame.grid(row=3, column=0, padx=20, pady=10, sticky="ew")
        
        self.btn_auto = ctk.CTkButton(self.auto_frame, text="▶ Start Auto-Pilot", fg_color="green", hover_color="darkgreen", command=self.toggle_autopilot)
        self.btn_auto.pack(padx=10, pady=10, fill="x")
        
        # Mouse Coordinates
        self.mouse_frame = ctk.CTkFrame(self.sidebar_frame)
        self.mouse_frame.grid(row=4, column=0, padx=20, pady=10, sticky="ew")
        
        ctk.CTkLabel(self.mouse_frame, text="Total Drag Distance", font=ctk.CTkFont(weight="bold")).pack(pady=(5, 0))
        self.mouse_coord_label = ctk.CTkLabel(self.mouse_frame, text="X: 0 | Y: 0", font=ctk.CTkFont(size=16, weight="bold"), text_color="cyan")
        self.mouse_coord_label.pack(pady=(5, 10))
        
        btn_reset = ctk.CTkButton(self.mouse_frame, text="Reset Tracker", width=100, height=24, command=self.reset_mouse_tracker)
        btn_reset.pack(pady=(0, 10))
        
        # Log Box
        self.log_box = ctk.CTkTextbox(self.sidebar_frame)
        self.log_box.grid(row=5, column=0, padx=20, pady=(10, 20), sticky="nsew")
        self.log_box.configure(state="disabled")
        
        # ── RIGHT VIDEO FEED ──
        self.video_frame = ctk.CTkFrame(self)
        self.video_frame.grid(row=0, column=1, padx=20, pady=20, sticky="nsew")
        
        self.video_label = ctk.CTkLabel(self.video_frame, text="Starting Webcam...")
        self.video_label.pack(expand=True, fill="both")

    def rebuild_virtual_pad(self):
        # Clear existing buttons
        for widget in self.pad_list_frame.winfo_children():
            widget.destroy()
            
        self.macro_buttons.clear()
            
        for index, macro in enumerate(self.text_macros):
            row_frame = ctk.CTkFrame(self.pad_list_frame, fg_color="transparent")
            row_frame.pack(fill="x", pady=2)
            
            # Main trigger button
            btn_text = f"Key {macro['key']}: {macro['name']}"
            trigger_btn = ctk.CTkButton(row_frame, text=btn_text, command=lambda k=macro['key']: self.send_command(k))
            trigger_btn.pack(side="left", expand=True, fill="x", padx=(0, 5))
            
            self.macro_buttons[macro['key']] = trigger_btn
            
            # Settings gear button
            settings_btn = ctk.CTkButton(row_frame, text="⚙️", width=30, command=lambda idx=index: self.open_settings(idx))
            settings_btn.pack(side="right")

    def open_settings(self, index):
        macro = self.text_macros[index]
        
        settings_window = ctk.CTkToplevel(self)
        settings_window.title(f"Settings - Key {macro['key']}")
        settings_window.geometry("350x350")
        settings_window.grab_set() # Make it modal
        
        # Name
        ctk.CTkLabel(settings_window, text="Macro Name:").pack(pady=(10, 0), padx=20, anchor="w")
        name_entry = ctk.CTkEntry(settings_window)
        name_entry.insert(0, macro.get("name", ""))
        name_entry.pack(pady=(0, 10), padx=20, fill="x")
        
        # Target Text
        ctk.CTkLabel(settings_window, text="OCR Target Text:").pack(pady=(10, 0), padx=20, anchor="w")
        text_entry = ctk.CTkEntry(settings_window)
        text_entry.insert(0, macro.get("text", ""))
        text_entry.pack(pady=(0, 10), padx=20, fill="x")
        
        # Confidence
        ctk.CTkLabel(settings_window, text="Confidence Threshold (%):").pack(pady=(10, 0), padx=20, anchor="w")
        conf_val = ctk.StringVar(value=str(macro.get("confidence", 80)))
        
        conf_label = ctk.CTkLabel(settings_window, textvariable=conf_val)
        conf_label.pack(pady=0, padx=20)
        
        def slider_event(value):
            conf_val.set(f"{int(value)}")
            
        conf_slider = ctk.CTkSlider(settings_window, from_=10, to=100, command=slider_event)
        conf_slider.set(macro.get("confidence", 80))
        conf_slider.pack(pady=(0, 10), padx=20, fill="x")
        
        # Save function
        def save():
            self.text_macros[index]["name"] = name_entry.get()
            self.text_macros[index]["text"] = text_entry.get()
            self.text_macros[index]["confidence"] = int(float(conf_slider.get()))
            
            self.save_text_macros()
            self.rebuild_virtual_pad()
            settings_window.destroy()
            
        save_btn = ctk.CTkButton(settings_window, text="Save Settings", command=save, fg_color="green", hover_color="darkgreen")
        save_btn.pack(pady=20, padx=20, fill="x")


    def log(self, msg):
        self.log_box.configure(state="normal")
        self.log_box.insert("end", f"[{time.strftime('%H:%M:%S')}] {msg}\n")
        self.log_box.see("end")
        self.log_box.configure(state="disabled")
        print(msg)

    def refresh_ports(self):
        ports = serial.tools.list_ports.comports()
        port_list = [p.device for p in ports]
        self.port_cb.configure(values=port_list)
        if port_list:
            for p in port_list:
                if "USB" in p or "ACM" in p:
                    self.port_var.set(p)
                    break
            else:
                self.port_var.set(port_list[0])
        else:
            self.port_var.set("No ports found")

    def toggle_connection(self):
        if self.serial_conn and self.serial_conn.is_open:
            self.keep_reading_serial = False
            if self.serial_thread:
                self.serial_thread.join(timeout=1)
                
            self.serial_conn.close()
            self.serial_conn = None
            self.btn_connect.configure(text="Connect", fg_color=['#3a7ebf', '#1f538d'])
            self.port_cb.configure(state="normal")
            self.log("Disconnected from ESP32.")
        else:
            port = self.port_var.get()
            try:
                self.serial_conn = serial.Serial(port, 921600, timeout=0.1)
                self.btn_connect.configure(text="Disconnect", fg_color="red", hover_color="darkred")
                self.port_cb.configure(state="disabled")
                self.log(f"Connected to ESP32 on {port}")
                
                self.keep_reading_serial = True
                self.serial_thread = threading.Thread(target=self.read_serial_loop, daemon=True)
                self.serial_thread.start()
            except Exception as e:
                self.log(f"Connection Error: {e}")

    def reset_mouse_tracker(self):
        self.total_dx = 0
        self.total_dy = 0
        self.mouse_coord_label.configure(text=f"X: {self.total_dx} | Y: {self.total_dy}")

    def read_serial_loop(self):
        while self.keep_reading_serial and self.serial_conn and self.serial_conn.is_open:
            try:
                line = self.serial_conn.readline()
                if line:
                    decoded = line.decode('utf-8', errors='ignore').strip()
                    if "[Mouse] dx:" in decoded:
                        # Extract dx and dy
                        parts = decoded.replace("[Mouse]", "").strip().split(",")
                        if len(parts) == 2:
                            dx_val = int(parts[0].replace("dx:", "").strip())
                            dy_val = int(parts[1].replace("dy:", "").strip())
                            
                            if self.is_dragging:
                                self.total_dx += dx_val
                                self.total_dy += dy_val
                                self.mouse_coord_label.configure(text=f"X: {self.total_dx} | Y: {self.total_dy}")
                                
                    elif "[Mouse] Left Click (Pressed)" in decoded:
                        self.is_dragging = True
                        self.total_dx = 0
                        self.total_dy = 0
                        self.mouse_coord_label.configure(text=f"X: 0 | Y: 0")
                        self.log("Started tracking drag...")
                        
                    elif "[Mouse] Left Click (Released)" in decoded:
                        if self.is_dragging:
                            self.is_dragging = False
                            self.log(f"Drag completed! Total distance: X={self.total_dx}, Y={self.total_dy}")
                            
                    elif decoded != "":
                        # Standard log
                        self.log(f"ESP32: {decoded}")
            except Exception as e:
                pass
            time.sleep(0.01)

    def flash_button(self, key):
        if key in self.macro_buttons:
            btn = self.macro_buttons[key]
            orig_color = btn.cget("fg_color")
            btn.configure(fg_color="green")
            self.after(500, lambda: btn.configure(fg_color=['#3a7ebf', '#1f538d']))

    def send_command(self, key):
        if self.serial_conn and self.serial_conn.is_open:
            try:
                self.serial_conn.write(key.encode('utf-8') + b'\n')
                self.log(f"Sent command: '{key}'")
                self.flash_button(key)
            except Exception as e:
                self.log(f"Send Error: {e}")
                self.toggle_connection()
        else:
            self.log("ESP32 not connected!")

    def toggle_autopilot(self):
        if self.auto_pilot_running:
            self.auto_pilot_running = False
            self.btn_auto.configure(text="▶ Start Auto-Pilot", fg_color="green", hover_color="darkgreen")
            self.log("Auto-Pilot STOPPED.")
        else:
            self.auto_pilot_running = True
            self.btn_auto.configure(text="⏹ Stop Auto-Pilot", fg_color="red", hover_color="darkred")
            self.log("Auto-Pilot STARTED.")

    def start_camera(self):
        self.cap = cv2.VideoCapture(0)
        self.update_video()

    def process_ocr_thread(self, frame, current_time):
        try:
            gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
            
            # Pre-processing for webcam text:
            # 1. Upscale 1.5x to make small phone text readable for Tesseract (faster than 2x)
            gray_large = cv2.resize(gray, None, fx=1.5, fy=1.5, interpolation=cv2.INTER_LINEAR)
            
            # 2. Blur slightly to remove phone screen pixel grid (moiré effect)
            gray_blur = cv2.GaussianBlur(gray_large, (3, 3), 0)
            
            # 3. Increase contrast (CLAHE) to fight screen glare
            clahe = cv2.createCLAHE(clipLimit=2.0, tileGridSize=(8,8))
            gray_contrast = clahe.apply(gray_blur)
            
            # PSM 11 = "Sparse text. Find as much text as possible in no particular order."
            # This stops Tesseract from getting confused by UI boxes and treating them as paragraphs.
            ocr_data = pytesseract.image_to_data(gray_contrast, output_type=pytesseract.Output.DICT, config='--psm 11')
            n_boxes = len(ocr_data['text'])
            
            valid_words = []
            current_detected_words = []
            
            for i in range(n_boxes):
                w = ocr_data['text'][i].strip()
                # Filter out noise (like wood grain) by requiring at least > 40 confidence
                if len(w) >= 2 and int(ocr_data['conf'][i]) > 40:
                    valid_words.append(w.lower())
                    # Divide coordinates by 1.5 because we upscaled the image 1.5x
                    x = int(ocr_data['left'][i] / 1.5)
                    y = int(ocr_data['top'][i] / 1.5)
                    w_box = int(ocr_data['width'][i] / 1.5)
                    h_box = int(ocr_data['height'][i] / 1.5)
                    current_detected_words.append((x, y, w_box, h_box, w))
                    
            self.last_detected_words = current_detected_words
            full_text = " ".join(valid_words)
            
            for macro in self.text_macros:
                target = macro.get("text", "")
                if not target:
                    continue
                    
                target_words = [w.lower() for w in target.split()]
                if not target_words:
                    continue
                    
                req_conf = macro.get("confidence", 80)
                is_match = False
                # Partial or full word match required based on percentage slider
                matched_count = sum(1 for tw in target_words if tw in full_text)
                match_percent = (matched_count / len(target_words)) * 100
                if match_percent >= req_conf:
                    is_match = True
                        
                if is_match:
                    self.last_match_text = f"MATCH: {macro['name']}"
                    self.last_match_time = current_time
                    
                    trigger_key = macro["key"]
                    if trigger_key != self.last_triggered_key and (current_time - self.debounce_time > 2.0):
                        self.send_command(trigger_key)
                        self.last_triggered_key = trigger_key
                        self.debounce_time = current_time
        except Exception as e:
            pass # Ignore OCR errors
        finally:
            self.ocr_thread_running = False

    def process_ocr(self, frame):
        current_time = time.time()
        
        # Only process every 0.1s to keep the thread running constantly
        if not hasattr(self, 'last_ocr_time'):
            self.last_ocr_time = 0
            self.ocr_thread_running = False
            
        if current_time - self.last_ocr_time >= 0.1 and not self.ocr_thread_running:
            self.last_ocr_time = current_time
            self.ocr_thread_running = True
            
            frame_copy = frame.copy()
            threading.Thread(target=self.process_ocr_thread, args=(frame_copy, current_time), daemon=True).start()
                
        # Draw the saved detections on EVERY frame so it doesn't flicker
        for (x, y, w_box, h_box, w) in self.last_detected_words:
            cv2.rectangle(frame, (x, y), (x + w_box, y + h_box), (255, 0, 0), 1)
            cv2.putText(frame, w, (x, y - 5), cv2.FONT_HERSHEY_SIMPLEX, 0.4, (255, 0, 0), 1)
            
        if current_time - self.last_match_time < 2.0:
            cv2.putText(frame, self.last_match_text, (50, 50), cv2.FONT_HERSHEY_SIMPLEX, 1.0, (0, 255, 0), 3)
            
        return frame

    def update_video(self):
        if self.cap and self.cap.isOpened():
            ret, frame = self.cap.read()
            if ret:
                if self.auto_pilot_running:
                    frame = self.process_ocr(frame)
                    
                # Convert to CTkImage format
                frame_rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
                pil_img = Image.fromarray(frame_rgb)
                
                # Resize dynamically to fit frame
                w, h = self.video_frame.winfo_width(), self.video_frame.winfo_height()
                if w > 10 and h > 10:
                    pil_img.thumbnail((w, h))
                    
                ctk_img = ctk.CTkImage(light_image=pil_img, dark_image=pil_img, size=(pil_img.width, pil_img.height))
                self.video_label.configure(image=ctk_img, text="")
                
        # Loop at ~30 FPS
        self.after(33, self.update_video)

    def on_closing(self):
        self.keep_reading_serial = False
        if self.cap:
            self.cap.release()
        if self.serial_conn and self.serial_conn.is_open:
            self.serial_conn.close()
        self.destroy()

if __name__ == "__main__":
    app = NativeAutomator()
    app.protocol("WM_DELETE_WINDOW", app.on_closing)
    app.mainloop()
