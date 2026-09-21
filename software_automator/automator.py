import cv2
import mss
import numpy as np
import serial
import serial.tools.list_ports
import time
import os
import glob
import subprocess
import threading
import tkinter as tk
from tkinter import ttk, scrolledtext, messagebox
import subprocess
import sys

# Configuration
CONFIDENCE_THRESHOLD = 0.85
CHECK_INTERVAL_SEC = 0.5   # Optimized: 2 scans per second = almost zero CPU load
BAUD_RATE = 115200
TEMPLATE_DIR = "templates"

class AutomatorGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("Macro Keyboard - Auto-Pilot")
        self.root.geometry("600x550")
        
        self.serial_conn = None
        self.templates = []
        
        self.auto_pilot_running = False
        self.auto_pilot_thread = None
        self.stop_event = threading.Event()
        self.uxplay_proc = None
        self.latest_frame = None

        self._create_templates_dir()
        self._build_ui()
        self.refresh_ports()
        self.load_templates()
        self.update_cv2_window()

    def _create_templates_dir(self):
        if not os.path.exists(TEMPLATE_DIR):
            os.makedirs(TEMPLATE_DIR)

    def update_cv2_window(self):
        if self.auto_pilot_running and self.latest_frame is not None:
            cv2.imshow("Auto-Pilot Live Vision", self.latest_frame)
            cv2.waitKey(1)
        else:
            try:
                if cv2.getWindowProperty("Auto-Pilot Live Vision", cv2.WND_PROP_VISIBLE) >= 0:
                    cv2.destroyWindow("Auto-Pilot Live Vision")
            except:
                pass
        self.root.after(100, self.update_cv2_window)

    def launch_mirror(self):
        if self.uxplay_proc is None or self.uxplay_proc.poll() is not None:
            self.log("Starting UxPlay mirror server...")
            try:
                self.uxplay_proc = subprocess.Popen(["uxplay"])
                self.log("UxPlay started! Connect via your iPhone Control Center.")
            except FileNotFoundError:
                self.log("Error: UxPlay is not installed! Run: sudo apt install uxplay")
        else:
            self.log("UxPlay is already running.")

    def _build_ui(self):
        # ── CONNECTION FRAME ──
        conn_frame = ttk.LabelFrame(self.root, text="ESP32 Connection")
        conn_frame.pack(fill="x", padx=10, pady=5)

        self.port_var = tk.StringVar()
        self.port_cb = ttk.Combobox(conn_frame, textvariable=self.port_var, state="readonly", width=30)
        self.port_cb.pack(side="left", padx=5, pady=5)

        ttk.Button(conn_frame, text="Refresh", command=self.refresh_ports).pack(side="left", padx=5)
        self.btn_connect = ttk.Button(conn_frame, text="Connect", command=self.toggle_connection)
        self.btn_connect.pack(side="left", padx=5)

        # ── VIRTUAL MACRO PAD ──
        pad_frame = ttk.LabelFrame(self.root, text="Virtual Macro Pad (Manual Trigger)")
        pad_frame.pack(fill="x", padx=10, pady=5)

        # 3x4 Layout roughly
        btn_layout = [
            ('1 (Names)', '1'), ('2 (Form)', '2'), ('3 (Username)', '3'),
            ('4 (Pwd)', '4'),   ('5 (Notes)', '5'), ('6 (Clear Hist)', '6'),
            ('7 (Timezone)', '7'),('0 (New Prof)', '0')
        ]
        
        grid_f = ttk.Frame(pad_frame)
        grid_f.pack(pady=5)
        
        row, col = 0, 0
        for text, key in btn_layout:
            b = ttk.Button(grid_f, text=text, width=15, command=lambda k=key: self.send_command(k))
            b.grid(row=row, column=col, padx=2, pady=2)
            col += 1
            if col > 2:
                col = 0
                row += 1

        # ── AUTO-PILOT CONTROL ──
        # ── MIRRORING FRAME ──
        mirror_frame = ttk.LabelFrame(self.root, text="iPhone Mirroring")
        mirror_frame.pack(fill="x", padx=10, pady=5)
        ttk.Button(mirror_frame, text="📱 Launch Screen Mirror (UxPlay)", command=self.launch_mirror).pack(side="left", padx=5, pady=5)

        auto_frame = ttk.LabelFrame(self.root, text="Computer Vision Auto-Pilot")
        auto_frame.pack(fill="x", padx=10, pady=5)

        btn_f = ttk.Frame(auto_frame)
        btn_f.pack(pady=5)

        ttk.Button(btn_f, text="Open Templates Folder", command=self.open_templates_folder).pack(side="left", padx=5)
        ttk.Button(btn_f, text="Reload Templates", command=self.load_templates).pack(side="left", padx=5)
        
        self.btn_auto = ttk.Button(btn_f, text="▶ Start Auto-Pilot", command=self.toggle_autopilot, state="disabled")
        self.btn_auto.pack(side="left", padx=5)

        # ── LOG CONSOLE ──
        log_frame = ttk.LabelFrame(self.root, text="System Log")
        log_frame.pack(fill="both", expand=True, padx=10, pady=5)
        
        self.log_txt = scrolledtext.ScrolledText(log_frame, wrap=tk.WORD, height=10)
        self.log_txt.pack(fill="both", expand=True, padx=5, pady=5)
        self.log_txt.config(state="disabled")

        self.log("System initialized. Select a port and connect.")

    def log(self, msg):
        self.log_txt.config(state="normal")
        self.log_txt.insert(tk.END, f"[{time.strftime('%H:%M:%S')}] {msg}\n")
        self.log_txt.see(tk.END)
        self.log_txt.config(state="disabled")

    def refresh_ports(self):
        ports = serial.tools.list_ports.comports()
        port_list = [p.device for p in ports]
        self.port_cb['values'] = port_list
        if port_list:
            # Try to auto-select likely USB ports
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
            # Disconnect
            self.stop_autopilot()
            self.serial_conn.close()
            self.serial_conn = None
            self.btn_connect.config(text="Connect")
            self.port_cb.config(state="readonly")
            self.btn_auto.config(state="disabled")
            self.log("Disconnected from ESP32.")
        else:
            # Connect
            port = self.port_var.get()
            try:
                self.serial_conn = serial.Serial(port, BAUD_RATE, timeout=1)
                self.btn_connect.config(text="Disconnect")
                self.port_cb.config(state="disabled")
                self.btn_auto.config(state="normal")
                self.log(f"Connected to ESP32 on {port}")
            except Exception as e:
                messagebox.showerror("Connection Error", f"Failed to connect:\n{e}")

    def send_command(self, key):
        if self.serial_conn and self.serial_conn.is_open:
            try:
                self.serial_conn.write(key.encode('utf-8'))
                self.log(f"Sent physical command: '{key}'")
            except Exception as e:
                self.log(f"Send Error: {e}")
                self.toggle_connection() # force disconnect
        else:
            self.log("Cannot send command: ESP32 is not connected!")

    def open_templates_folder(self):
        abs_path = os.path.abspath(TEMPLATE_DIR)
        if sys.platform == "win32":
            os.startfile(abs_path)
        elif sys.platform == "darwin":
            subprocess.Popen(["open", abs_path])
        else:
            subprocess.Popen(["xdg-open", abs_path])
        self.log("Opened templates folder.")

    def load_templates(self):
        self.templates = []
        files = glob.glob(os.path.join(TEMPLATE_DIR, "*.png"))
        for file in files:
            filename = os.path.basename(file)
            trigger_key = filename[0]
            img = cv2.imread(file, cv2.IMREAD_GRAYSCALE)
            if img is not None:
                # Downscale by 50% for 400% faster matching performance
                img = cv2.resize(img, (0, 0), fx=0.5, fy=0.5)
                self.templates.append({
                    "name": filename,
                    "key": trigger_key,
                    "image": img
                })
        self.log(f"Loaded {len(self.templates)} template(s) for auto-pilot.")

    def toggle_autopilot(self):
        if self.auto_pilot_running:
            self.stop_autopilot()
        else:
            self.start_autopilot()

    def start_autopilot(self):
        if not self.templates:
            messagebox.showwarning("No Templates", "No templates loaded! Please add .png files to the templates folder and reload.")
            return

        self.auto_pilot_running = True
        self.btn_auto.config(text="⏹ Stop Auto-Pilot")
        self.stop_event.clear()
        self.auto_pilot_thread = threading.Thread(target=self.cv_loop, daemon=True)
        self.auto_pilot_thread.start()
        self.log("Auto-Pilot STARTED. Watching screen...")

    def stop_autopilot(self):
        if self.auto_pilot_running:
            self.auto_pilot_running = False
            self.stop_event.set()
            if self.auto_pilot_thread:
                self.auto_pilot_thread.join(timeout=1.0)
            try:
                cv2.destroyAllWindows()
            except:
                pass
            self.btn_auto.config(text="▶ Start Auto-Pilot")
            self.log("Auto-Pilot STOPPED.")

    def cv_loop(self):
        last_triggered_key = None
        debounce_time = 0

        with mss.mss() as sct:
            # Try to grab the primary monitor (or entire virtual screen on some X11 setups)
            monitor = sct.monitors[1] if len(sct.monitors) > 1 else sct.monitors[0]
            
            while not self.stop_event.is_set():
                try:
                    sct_img = sct.grab(monitor)
                    screen_img = np.array(sct_img)

                    screen_gray = cv2.cvtColor(screen_img, cv2.COLOR_BGRA2GRAY)
                    
                    # Downscale the screen by 50% to match templates and run 4x faster
                    screen_gray_small = cv2.resize(screen_gray, (0, 0), fx=0.5, fy=0.5)
                    
                    display_img = cv2.cvtColor(screen_img, cv2.COLOR_BGRA2BGR)

                    current_time = time.time()
                    match_found = False

                    for tmpl in self.templates:
                        if self.stop_event.is_set():
                            break

                        res = cv2.matchTemplate(screen_gray_small, tmpl["image"], cv2.TM_CCOEFF_NORMED)
                        min_val, max_val, min_loc, max_loc = cv2.minMaxLoc(res)

                        if max_val >= CONFIDENCE_THRESHOLD:
                            # Draw a bright green bounding box for visual verification
                            # Multiply coordinates by 2 to map back to original screen size
                            h, w = tmpl["image"].shape
                            top_left = (max_loc[0] * 2, max_loc[1] * 2)
                            bottom_right = (top_left[0] + w * 2, top_left[1] + h * 2)
                            cv2.rectangle(display_img, top_left, bottom_right, (0, 255, 0), 4)
                            cv2.putText(display_img, f"MATCH: {tmpl['name']}", (top_left[0], top_left[1] - 10), cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 255, 0), 2)

                            trigger_key = tmpl["key"]

                            # Debounce check
                            if trigger_key != last_triggered_key or (current_time - debounce_time > 5.0):
                                self.root.after(0, self.log, f"MATCH: {tmpl['name']} (Conf: {max_val:.2f})")
                                self.root.after(0, self.send_command, trigger_key)

                                last_triggered_key = trigger_key
                                debounce_time = current_time
                                match_found = True

                            break # stop checking other templates this frame

                    # Show Live Vision Feed (Downscaled for performance)
                    scale_percent = 50
                    width = int(display_img.shape[1] * scale_percent / 100)
                    height = int(display_img.shape[0] * scale_percent / 100)
                    dim = (width, height)
                    resized = cv2.resize(display_img, dim, interpolation=cv2.INTER_AREA)

                    self.latest_frame = resized

                    if match_found:
                        time.sleep(1.5)  # Pause briefly after a match to let ESP32 execute
                    else:
                        time.sleep(CHECK_INTERVAL_SEC)

                except Exception as e:
                    self.root.after(0, self.log, f"CV Loop Error: {e}")
                    time.sleep(1)

    def on_closing(self):
        self.stop_autopilot()
        if self.uxplay_proc and self.uxplay_proc.poll() is None:
            self.uxplay_proc.terminate()
        if self.serial_conn and self.serial_conn.is_open:
            self.serial_conn.close()
        self.root.destroy()

if __name__ == "__main__":
    root = tk.Tk()
    app = AutomatorGUI(root)
    root.protocol("WM_DELETE_WINDOW", app.on_closing)
    root.mainloop()
