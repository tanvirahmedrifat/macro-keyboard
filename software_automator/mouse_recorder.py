import customtkinter as ctk
import serial
import serial.tools.list_ports
import json
import time
import threading

ctk.set_appearance_mode("Dark")
ctk.set_default_color_theme("blue")

class MouseRecorder(ctk.CTk):
    def __init__(self):
        super().__init__()
        self.title("Mouse Macro Recorder & Virtual Trackpad")
        self.geometry("1200x950")
        
        self.serial_conn = None
        self.is_recording = False
        self.is_replaying = False
        self.recording_data = []
        self.start_time = 0
        
        self.keep_reading = False
        self.read_thread = None
        
        # Trackpad variables
        self.last_tx = None
        self.last_ty = None
        self.frac_x = 0.0
        self.frac_y = 0.0
        
        # Drawing colors
        self.action_index = 0
        self.action_colors = ["#FF2B2B", "#3498DB", "#9B59B6", "#E67E22", "#1ABC9C", "#E84393", "#00CEC9"]
        self.current_color = self.action_colors[0]
        
        self.build_ui()
        self.refresh_ports()

    def build_ui(self):
        # ── LEFT FRAME (CONTROLS) ──
        self.left_frame = ctk.CTkFrame(self, fg_color="transparent")
        self.left_frame.pack(side="left", fill="y", padx=(20, 10), pady=20)
        
        # ── MIDDLE FRAME (iPHONE) ──
        self.middle_frame = ctk.CTkFrame(self, fg_color="transparent")
        self.middle_frame.pack(side="left", fill="both", expand=True, padx=10, pady=20)
        
        self.conn_frame = ctk.CTkFrame(self.left_frame)
        self.conn_frame.pack(pady=(0, 20), padx=0, fill="x")
        
        self.port_var = ctk.StringVar(value="Select Port")
        self.port_cb = ctk.CTkComboBox(self.conn_frame, variable=self.port_var, values=[])
        self.port_cb.pack(pady=10, padx=10, fill="x")
        
        self.btn_connect = ctk.CTkButton(self.conn_frame, text="Connect", command=self.toggle_connection)
        self.btn_connect.pack(pady=10, padx=10, fill="x")
        
        self.control_frame = ctk.CTkFrame(self.left_frame)
        self.control_frame.pack(pady=0, padx=0, fill="x")
        
        self.btn_record = ctk.CTkButton(self.control_frame, text="⏺ Start Recording", fg_color="red", hover_color="darkred", command=self.toggle_recording)
        self.btn_record.pack(pady=10, padx=10, fill="x")
        
        self.btn_replay = ctk.CTkButton(self.control_frame, text="▶ Replay Recording", fg_color="green", hover_color="darkgreen", command=self.replay_recording)
        self.btn_replay.pack(pady=10, padx=10, fill="x")
        
        self.btn_reset_ptr = ctk.CTkButton(self.control_frame, text="↖️ Reset Pointer to Top-Left", fg_color="#E67E22", hover_color="#D35400", command=self.reset_pointer)
        self.btn_reset_ptr.pack(pady=(0, 10), padx=10, fill="x")
        
        self.btn_save = ctk.CTkButton(self.control_frame, text="💾 Save to File", command=self.save_recording)
        self.btn_save.pack(pady=10, padx=10, side="left", expand=True)
        
        self.btn_load = ctk.CTkButton(self.control_frame, text="📂 Load from File", command=self.load_recording)
        self.btn_load.pack(pady=10, padx=10, side="right", expand=True)
        
        # Calibration Slider
        calib_frame = ctk.CTkFrame(self.control_frame, fg_color="transparent")
        calib_frame.pack(pady=5, fill="x")
        ctk.CTkLabel(calib_frame, text="Phone Calibration Multiplier", font=ctk.CTkFont(weight="bold")).pack()
        self.sensitivity_var = ctk.DoubleVar(value=1.0)
        self.sensitivity_slider = ctk.CTkSlider(calib_frame, from_=0.1, to=3.0, variable=self.sensitivity_var)
        self.sensitivity_slider.pack(pady=2, padx=10, fill="x")
        self.lbl_sens = ctk.CTkLabel(calib_frame, text="1.00x")
        self.lbl_sens.pack()
        self.sensitivity_slider.configure(command=lambda v: self.lbl_sens.configure(text=f"{v:.2f}x"))
        
        # Replay Speed Slider
        speed_frame = ctk.CTkFrame(self.control_frame, fg_color="transparent")
        speed_frame.pack(pady=5, fill="x")
        ctk.CTkLabel(speed_frame, text="Replay Speed", font=ctk.CTkFont(weight="bold")).pack()
        self.speed_var = ctk.DoubleVar(value=1.0)
        self.speed_slider = ctk.CTkSlider(speed_frame, from_=0.1, to=3.0, variable=self.speed_var)
        self.speed_slider.pack(pady=2, padx=10, fill="x")
        self.lbl_speed = ctk.CTkLabel(speed_frame, text="1.00x")
        self.lbl_speed.pack()
        self.speed_slider.configure(command=lambda v: self.lbl_speed.configure(text=f"{v:.2f}x"))
        
        # ── iPHONE 12 WHITEBOARD SIMULATOR ──
        self.iphone_frame = ctk.CTkFrame(self.middle_frame)
        self.iphone_frame.pack(pady=0, padx=0, fill="both", expand=True)
        
        ctk.CTkLabel(self.iphone_frame, text="📱 iPhone 12 Macro Whiteboard", font=ctk.CTkFont(weight="bold")).pack(pady=5)
        
        self.btn_clear = ctk.CTkButton(self.iphone_frame, text="🧹 Clear Whiteboard", command=self.clear_whiteboard, fg_color="gray", height=24)
        self.btn_clear.pack(pady=(0, 5))
        
        import tkinter as tk
        # iPhone 12 exact logical resolution (390x844)
        self.canvas_width = 390
        self.canvas_height = 844
        self.canvas = tk.Canvas(self.iphone_frame, width=self.canvas_width, height=self.canvas_height, bg="white", cursor="crosshair", highlightthickness=2, highlightbackground="#333333")
        self.canvas.pack(pady=10, expand=False)
        
        # Draw iPhone Notch
        self.draw_notch()
        
        self.canvas.bind("<ButtonPress-1>", self.trackpad_press)
        self.canvas.bind("<ButtonRelease-1>", self.trackpad_release)
        self.canvas.bind("<B1-Motion>", self.trackpad_drag)
        
        # ── RIGHT FRAME (LOGS) ──
        self.right_frame = ctk.CTkFrame(self)
        self.right_frame.pack(side="right", fill="both", expand=True, padx=(0, 20), pady=20)
        
        ctk.CTkLabel(self.right_frame, text="System Logs", font=ctk.CTkFont(weight="bold")).pack(pady=10)
        
        self.log_box = ctk.CTkTextbox(self.right_frame)
        self.log_box.pack(pady=10, padx=10, fill="both", expand=True)
        self.log("Ready.")

    def draw_notch(self):
        notch_width = 140
        notch_height = 25
        cx = self.canvas_width / 2
        self.canvas.create_rectangle(cx - notch_width/2, 0, cx + notch_width/2, notch_height, fill="black", outline="black", tags="notch")
        # Add a speaker grill and camera dot
        self.canvas.create_line(cx - 20, 12, cx + 20, 12, fill="#333333", width=4, capstyle="round", tags="notch")
        self.canvas.create_oval(cx + 40, 8, cx + 48, 16, fill="#222222", outline="#444444", tags="notch")

    def clear_whiteboard(self):
        self.canvas.delete("drawing")
        self.action_index = 0

    def trackpad_press(self, event):
        self.last_tx = event.x
        self.last_ty = event.y
        
        self.current_color = self.action_colors[self.action_index % len(self.action_colors)]
        self.action_index += 1
        
        # Draw Start Marker (Circle)
        r = 6
        self.canvas.create_oval(event.x-r, event.y-r, event.x+r, event.y+r, fill=self.current_color, outline="white", width=1, tags="drawing")
        
        if self.is_recording:
            self.recording_data.append({"t": time.time() - self.start_time, "type": "drag_start"})
        if self.serial_conn and self.serial_conn.is_open:
            try:
                self.serial_conn.write(b"D\n") # Drag Start
            except: pass

    def trackpad_release(self, event):
        # Draw End Marker (Square)
        if self.last_tx is not None and self.last_ty is not None:
            r = 6
            self.canvas.create_rectangle(self.last_tx-r, self.last_ty-r, self.last_tx+r, self.last_ty+r, fill=self.current_color, outline="white", width=1, tags="drawing")
            
        self.last_tx = None
        self.last_ty = None
        
        if self.is_recording:
            self.recording_data.append({"t": time.time() - self.start_time, "type": "drag_end"})
        if self.serial_conn and self.serial_conn.is_open:
            try:
                self.serial_conn.write(b"U\n") # Drag End
            except: pass

    def trackpad_drag(self, event):
        if self.last_tx is not None and self.last_ty is not None:
            # Draw line on whiteboard
            self.canvas.create_line(self.last_tx, self.last_ty, event.x, event.y, fill=self.current_color, width=3, capstyle="round", smooth=True, tags="drawing")
            
            mult = self.sensitivity_var.get()
            raw_dx = (event.x - self.last_tx) * mult + self.frac_x
            raw_dy = (event.y - self.last_ty) * mult + self.frac_y
            
            dx = int(raw_dx)
            dy = int(raw_dy)
            
            self.frac_x = raw_dx - dx
            self.frac_y = raw_dy - dy
            
            # Clamp to int8_t limits
            dx = max(-127, min(127, dx))
            dy = max(-127, min(127, dy))
            
            # Send movement if it's large enough to avoid spamming 0s
            if abs(dx) > 0 or abs(dy) > 0:
                if self.is_recording:
                    self.recording_data.append({"t": time.time() - self.start_time, "type": "move", "dx": dx, "dy": dy})
                if self.serial_conn and self.serial_conn.is_open:
                    try:
                        self.serial_conn.write(f"M{dx},{dy}\n".encode())
                    except: pass
                    
            self.last_tx = event.x
            self.last_ty = event.y

    def log(self, msg):
        self.log_box.insert("end", f"{msg}\n")
        self.log_box.see("end")

    def refresh_ports(self):
        ports = serial.tools.list_ports.comports()
        port_list = [p.device for p in ports if "USB" in p.device or "ACM" in p.device]
        self.port_cb.configure(values=port_list)
        if port_list:
            self.port_var.set(port_list[0])

    def toggle_connection(self):
        if self.serial_conn and self.serial_conn.is_open:
            self.keep_reading = False
            self.serial_conn.close()
            self.serial_conn = None
            self.btn_connect.configure(text="Connect", fg_color=['#3a7ebf', '#1f538d'])
            self.log("Disconnected.")
        else:
            try:
                self.serial_conn = serial.Serial(self.port_var.get(), 115200, timeout=0.1)
                self.btn_connect.configure(text="Disconnect", fg_color="gray")
                self.log(f"Connected to {self.port_var.get()}")
                self.keep_reading = True
                self.read_thread = threading.Thread(target=self.serial_loop, daemon=True)
                self.read_thread.start()
            except Exception as e:
                self.log(f"Error: {e}")

    def serial_loop(self):
        while self.keep_reading and self.serial_conn and self.serial_conn.is_open:
            try:
                line = self.serial_conn.readline()
                if line and self.is_recording:
                    decoded = line.decode('utf-8', errors='ignore').strip()
                    now = time.time() - self.start_time
                    
                    if "[Mouse] dx:" in decoded:
                        parts = decoded.replace("[Mouse]", "").strip().split(",")
                        if len(parts) == 2:
                            dx = int(parts[0].replace("dx:", "").strip())
                            dy = int(parts[1].replace("dy:", "").strip())
                            self.recording_data.append({"t": now, "type": "move", "dx": dx, "dy": dy})
                            
                    elif "[Mouse] Left Click (Pressed)" in decoded:
                        self.recording_data.append({"t": now, "type": "drag_start"})
                        
                    elif "[Mouse] Left Click (Released)" in decoded:
                        self.recording_data.append({"t": now, "type": "drag_end"})
                        
            except Exception:
                pass
            time.sleep(0.005)

    def toggle_recording(self):
        if self.is_recording:
            self.is_recording = False
            self.btn_record.configure(text="⏺ Start Recording", fg_color="red")
            self.log(f"Recording stopped. Captured {len(self.recording_data)} events.")
        else:
            self.recording_data = []
            self.start_time = time.time()
            self.is_recording = True
            self.btn_record.configure(text="⏹ Stop Recording", fg_color="gray")
            self.log("Recording started! Move the mouse on the device...")

    def reset_pointer(self):
        if not self.serial_conn or not self.serial_conn.is_open:
            self.log("Connect to ESP32 first!")
            return
            
        self.log("Slamming pointer to top-left corner...")
        threading.Thread(target=self._reset_ptr_thread, daemon=True).start()
        
    def _reset_ptr_thread(self):
        try:
            # Send max safe speed for iOS to prevent it clamping to 1-pixel
            for _ in range(80):
                self.serial_conn.write(b"M-25,-25\n")
                time.sleep(0.015)
            self.log("Pointer reset complete! You can now start recording from (0,0).")
        except Exception as e:
            self.log(f"Reset error: {e}")

    def replay_recording(self):
        if not self.serial_conn or not self.serial_conn.is_open:
            self.log("Connect to ESP32 first!")
            return
        if not self.recording_data:
            self.log("No data to replay.")
            return
            
        self.log("Replaying...")
        threading.Thread(target=self._replay_thread, daemon=True).start()
        
    def _replay_thread(self):
        start_t = time.time()
        speed_mult = self.speed_var.get()
        if speed_mult <= 0: speed_mult = 1.0
        
        for ev in self.recording_data:
            # Wait until it's time to execute this event (adjusted by speed multiplier)
            target_time = ev["t"] / speed_mult
            while (time.time() - start_t) < target_time:
                time.sleep(0.001)
                
            try:
                if ev["type"] == "move":
                    cmd = f"M{ev['dx']},{ev['dy']}\n"
                    self.serial_conn.write(cmd.encode())
                elif ev["type"] == "drag_start":
                    self.serial_conn.write(b"D\n")
                elif ev["type"] == "drag_end":
                    self.serial_conn.write(b"U\n")
            except Exception as e:
                self.log(f"Replay error: {e}")
                break
        self.log("Replay finished!")

    def save_recording(self):
        try:
            with open("mouse_macro.json", "w") as f:
                json.dump(self.recording_data, f)
            self.log("Saved to mouse_macro.json")
        except Exception as e:
            self.log(f"Save error: {e}")

    def load_recording(self):
        try:
            with open("mouse_macro.json", "r") as f:
                self.recording_data = json.load(f)
            self.log(f"Loaded {len(self.recording_data)} events from mouse_macro.json")
        except Exception as e:
            self.log(f"Load error: {e}")

if __name__ == "__main__":
    app = MouseRecorder()
    app.mainloop()
