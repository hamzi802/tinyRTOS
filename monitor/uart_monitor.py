"""Live tinyRTOS UART dashboard. Requires pyserial."""

from __future__ import annotations

import csv
import io
import random
import threading
import time
import tkinter as tk
from tkinter import messagebox, ttk

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    serial = None
    list_ports = None


BG = "#0b1017"
PANEL = "#151f2b"
PANEL_ALT = "#1c2937"
LINE = "#2a3c4d"
TEXT = "#edf4fa"
MUTED = "#8293a5"
CYAN = "#58e1db"
BLUE = "#68a9ff"
AMBER = "#f5bd68"
RED = "#ff7474"
GREEN = "#76e4a8"


class UartMonitor(tk.Tk):
    def __init__(self) -> None:
        super().__init__()
        self.title("tinyRTOS | Mission Control")
        self.geometry("1240x780")
        self.minsize(980, 650)
        self.configure(bg=BG)
        self.protocol("WM_DELETE_WINDOW", self.close)

        self.connection = None
        self.reader_thread = None
        self.stop_reader = threading.Event()
        self.demo_job = None
        self.started_at = time.monotonic()
        self.messages = 0
        self.last_tick = None
        self.last_message_at = None
        self.tasks = {}
        self.tick_history = []
        self.switch_events = []
        self.animation_step = 0
        self.current_task = tk.StringVar(value="idle")
        self.switch_count = tk.StringVar(value="0")
        self.last_switch = tk.StringVar(value="--")

        self.port = tk.StringVar()
        self.baud = tk.StringVar(value="115200")
        self.link_text = tk.StringVar(value="DISCONNECTED")
        self.message_text = tk.StringVar(value="0")
        self.tick_text = tk.StringVar(value="--")
        self.rate_text = tk.StringVar(value="0.0 Hz")
        self.uptime_text = tk.StringVar(value="0 s")
        self.rx_state = tk.StringVar(value="Waiting for UART telemetry")
        self._style()
        self._build()
        self.refresh_ports()
        self.after(250, self.refresh_view)
        self.after(90, self.animate)

    def _style(self) -> None:
        style = ttk.Style(self)
        style.theme_use("clam")
        style.configure("TCombobox", fieldbackground=PANEL_ALT, background=PANEL_ALT,
                        foreground=TEXT, arrowcolor=CYAN, borderwidth=0)
        style.configure("Treeview", background=PANEL, fieldbackground=PANEL,
                        foreground=TEXT, rowheight=28, borderwidth=0)
        style.configure("Treeview.Heading", background=PANEL_ALT, foreground=MUTED,
                        relief="flat", font=("Segoe UI", 9, "bold"))
        style.map("Treeview", background=[("selected", "#294555")])

    def _build(self) -> None:
        header = tk.Frame(self, bg=BG)
        header.pack(fill="x", padx=30, pady=(26, 18))
        title = tk.Frame(header, bg=BG)
        title.pack(side="left")
        tk.Label(title, text="tiny", bg=BG, fg=TEXT,
                 font=("Segoe UI", 25, "bold")).pack(side="left")
        tk.Label(title, text="RTOS", bg=BG, fg=CYAN,
                 font=("Segoe UI", 25, "bold")).pack(side="left")
        tk.Label(title, text="  /  MISSION CONTROL", bg=BG, fg=MUTED,
                 font=("Segoe UI", 9, "bold")).pack(side="left", pady=(11, 0))
        tk.Label(header, text="Live observability for your Cortex-M4 scheduler",
                 bg=BG, fg=MUTED, font=("Segoe UI", 10)).pack(side="left", padx=(24, 0), pady=(13, 0))
        status = tk.Frame(header, bg=PANEL, padx=12, pady=8)
        status.pack(side="right")
        self.signal_canvas = tk.Canvas(status, width=24, height=24, bg=PANEL,
                                       highlightthickness=0)
        self.signal_canvas.pack(side="left", padx=(0, 8))
        tk.Label(status, textvariable=self.link_text, bg=PANEL, fg=GREEN,
                 font=("Segoe UI", 9, "bold")).pack(side="left")
        tk.Label(status, textvariable=self.rx_state, bg=PANEL, fg=MUTED,
                 font=("Segoe UI", 8)).pack(side="left", padx=(12, 0))

        controls = tk.Frame(self, bg=PANEL, padx=16, pady=12)
        controls.pack(fill="x", padx=30, pady=(0, 14))
        self._label(controls, "PORT").pack(side="left")
        self.port_box = ttk.Combobox(controls, textvariable=self.port, width=17, state="readonly")
        self.port_box.pack(side="left", padx=(8, 18))
        self._label(controls, "BAUD").pack(side="left")
        ttk.Combobox(controls, textvariable=self.baud, values=("9600", "115200", "230400"),
                     width=10, state="readonly").pack(side="left", padx=(8, 18))
        self.connect_button = tk.Button(controls, text="CONNECT", command=self.toggle_connection,
                                        bg=CYAN, fg="#081315", activebackground="#8af5ef",
                                        relief="flat", padx=16, pady=7, font=("Segoe UI", 9, "bold"))
        self.connect_button.pack(side="left")
        tk.Button(controls, text="REFRESH PORTS", command=self.refresh_ports, bg=PANEL_ALT, fg=TEXT,
                  activebackground="#304354", relief="flat", padx=12, pady=7).pack(side="left", padx=8)
        tk.Button(controls, text="DEMO STREAM", command=self.toggle_demo, bg=PANEL_ALT, fg=AMBER,
                activebackground="#304354", relief="flat", padx=12, pady=7).pack(side="left")

        cmdbar = tk.Frame(self, bg=PANEL, padx=16, pady=10)
        cmdbar.pack(fill="x", padx=30, pady=(0, 12))
        tk.Label(cmdbar, text="COMMAND CENTER", bg=PANEL, fg=MUTED,
                 font=("Segoe UI", 9, "bold")).pack(side="left")
        self.command_var = tk.StringVar()
        self.command_entry = tk.Entry(cmdbar, textvariable=self.command_var, width=35,
                                      bg=PANEL_ALT, fg=TEXT, insertbackground=TEXT,
                                      highlightthickness=0)
        self.command_entry.pack(side="left", padx=(10, 10), fill="x", expand=True)
        self.command_entry.bind("<Return>", self.send_command)
        tk.Button(cmdbar, text="SEND", command=self.send_command, bg=GREEN, fg="#0d1b12",
                  activebackground="#aaf0c5", relief="flat", padx=14, pady=7,
                  font=("Segoe UI", 9, "bold")).pack(side="right")
        for command in ("status", "tasklist", "toggle"):
            tk.Button(cmdbar, text=command, command=lambda value=command: self.quick_command(value),
                      bg=PANEL_ALT, fg=MUTED, activebackground=LINE,
                      activeforeground=TEXT, relief="flat", padx=8, pady=6,
                      font=("Consolas", 8)).pack(side="right", padx=(0, 6))

        stats = tk.Frame(self, bg=BG)
        stats.pack(fill="x", padx=30, pady=(0, 14))
        for title, variable, color in (("RUNTIME", self.uptime_text, BLUE),
                           ("MESSAGES RX", self.message_text, CYAN),
                                       ("LAST TICK", self.tick_text, AMBER),
                                       ("INPUT RATE", self.rate_text, GREEN),
                                       ("CURRENT TASK", self.current_task, GREEN)):
            card = tk.Frame(stats, bg=PANEL, padx=18, pady=13)
            card.pack(side="left", fill="x", expand=True, padx=(0, 10))
            tk.Label(card, text=title, bg=PANEL, fg=MUTED, font=("Segoe UI", 9, "bold")).pack(anchor="w")
            tk.Label(card, textvariable=variable, bg=PANEL, fg=color,
                 font=("Consolas", 20, "bold")).pack(anchor="w", pady=(5, 0))

        switch_panel = tk.Frame(self, bg=PANEL, padx=14, pady=12)
        switch_panel.pack(fill="x", padx=30, pady=(0, 12))
        tk.Label(switch_panel, text="LAST SWITCH", bg=PANEL, fg=MUTED,
                 font=("Segoe UI", 9, "bold")).pack(side="left")
        tk.Label(switch_panel, textvariable=self.last_switch, bg=PANEL, fg=AMBER,
                 font=("Consolas", 12, "bold")).pack(side="left", padx=(12, 24))
        tk.Label(switch_panel, text="SWITCH COUNT", bg=PANEL, fg=MUTED,
                 font=("Segoe UI", 9, "bold")).pack(side="left")
        tk.Label(switch_panel, textvariable=self.switch_count, bg=PANEL, fg=CYAN,
                 font=("Consolas", 12, "bold")).pack(side="left", padx=(12, 0))

        body = tk.Frame(self, bg=BG)
        body.pack(fill="both", expand=True, padx=30, pady=(0, 24))
        left = tk.Frame(body, bg=BG)
        left.pack(side="left", fill="both", expand=True, padx=(0, 12))
        tk.Label(left, text="TASK HEARTBEATS", bg=BG, fg=TEXT,
                 font=("Segoe UI", 10, "bold")).pack(anchor="w", pady=(0, 8))
        self.task_area = tk.Frame(left, bg=BG)
        self.task_area.pack(fill="x")
        delay_panel = tk.Frame(left, bg=PANEL, padx=14, pady=10)
        delay_panel.pack(fill="x", pady=(8, 0))
        tk.Label(delay_panel, text="TASK DELAYS (MS)", bg=PANEL, fg=MUTED,
                 font=("Segoe UI", 9, "bold")).pack(anchor="w")
        self.delay_inputs = {}
        for name, default in (("task1", 300), ("task2", 600), ("task3", 100)):
            row = tk.Frame(delay_panel, bg=PANEL)
            row.pack(fill="x", pady=(6, 0))
            tk.Label(row, text=name.upper(), width=9, anchor="w", bg=PANEL, fg=TEXT,
                     font=("Consolas", 10, "bold")).pack(side="left")
            value = tk.StringVar(value=str(default))
            self.delay_inputs[name] = value
            tk.Entry(row, textvariable=value, width=9, bg=PANEL_ALT, fg=TEXT,
                     insertbackground=TEXT, highlightthickness=0).pack(side="left")
            tk.Button(row, text="APPLY", command=lambda task=name: self.apply_delay(task),
                      bg=PANEL_ALT, fg=GREEN, activebackground=LINE, relief="flat",
                      padx=10, pady=3).pack(side="left", padx=(8, 0))
        tk.Label(left, text="SCHEDULER RHYTHM", bg=BG, fg=MUTED,
                 font=("Segoe UI", 10, "bold")).pack(anchor="w", pady=(22, 8))
        self.chart = tk.Canvas(left, height=145, bg=PANEL, highlightthickness=0)
        self.chart.pack(fill="x")

        right = tk.Frame(body, bg=PANEL, padx=14, pady=14)
        right.pack(side="right", fill="both", expand=True)
        tk.Label(right, text="SERIAL FEED", bg=PANEL, fg=MUTED,
                 font=("Segoe UI", 10, "bold")).pack(anchor="w", pady=(0, 8))
        columns = ("time", "kind", "payload")
        self.feed = ttk.Treeview(right, columns=columns, show="headings")
        self.feed.heading("time", text="TIME")
        self.feed.heading("kind", text="TYPE")
        self.feed.heading("payload", text="PAYLOAD")
        self.feed.column("time", width=75, stretch=False)
        self.feed.column("kind", width=100, stretch=False)
        self.feed.column("payload", width=260)
        self.feed.pack(fill="both", expand=True)

    def _label(self, parent: tk.Widget, text: str) -> tk.Label:
        return tk.Label(parent, text=text, bg=PANEL, fg=MUTED, font=("Segoe UI", 9, "bold"))

    def refresh_ports(self) -> None:
        port_info = list(list_ports.comports()) if list_ports else []
        ports = [port.device for port in port_info]
        self.port_box["values"] = ports
        preferred = next((port.device for port in port_info
                          if "FTDI" in (port.description or "").upper()), None)
        if preferred:
            self.port.set(preferred)
        elif ports and self.port.get() not in ports:
            self.port.set(ports[0])

    def toggle_connection(self) -> None:
        if self.connection:
            self.disconnect()
            return
        if serial is None:
            messagebox.showerror("pyserial is missing", "Install dependencies with: py -m pip install -r monitor/requirements.txt")
            return
        if not self.port.get():
            messagebox.showwarning("Select a port", "Connect the USB-to-TTL adapter and choose its COM port first.")
            return
        try:
            self.connection = serial.Serial(self.port.get(), int(self.baud.get()), timeout=0.2)
        except serial.SerialException as error:
            messagebox.showerror("Could not open port", str(error))
            return
        self.stop_reader.clear()
        self.reader_thread = threading.Thread(target=self.read_serial, daemon=True)
        self.reader_thread.start()
        self.set_link(True)

    def disconnect(self) -> None:
        self.stop_reader.set()
        if self.connection:
            self.connection.close()
        self.connection = None
        self.set_link(False)

    def read_serial(self) -> None:
        while not self.stop_reader.is_set() and self.connection:
            try:
                line = self.connection.readline().decode("utf-8", errors="replace").strip()
            except (serial.SerialException, OSError):
                break
            if line:
                self.after(0, self.handle_line, line)

    def toggle_demo(self) -> None:
        if self.demo_job:
            self.after_cancel(self.demo_job)
            self.demo_job = None
            self.set_link(False)
            return
        self.set_link(True, "DEMO STREAM")
        self.demo_line()

    def demo_line(self) -> None:
        names = ("task1", "task2", "task3")
        name = random.choices(names, weights=(3, 2, 6))[0]
        tick = int((time.monotonic() - self.started_at) * 1000)
        self.handle_line(f"TASK,{name},{tick},HEARTBEAT")
        self.demo_job = self.after(random.randint(80, 340), self.demo_line)

    def handle_line(self, line: str) -> None:
        now = time.monotonic()
        self.messages += 1
        self.message_text.set(str(self.messages))
        self.rx_state.set("Receiving telemetry")
        try:
            fields = next(csv.reader(io.StringIO(line)))
        except (csv.Error, StopIteration):
            fields = []
        if len(fields) >= 4 and fields[0] == "TASK":
            name, tick, state = fields[1], fields[2], fields[3]
            try:
                tick_value = int(tick)
            except ValueError:
                tick_value = 0
            self.tasks[name] = {"tick": tick_value, "state": state, "seen": now}
            self.tick_text.set(f"{tick_value:,}")
            self.tick_history.append((now, tick_value))
            self.tick_history = self.tick_history[-70:]
            self.current_task.set(name)

        elif len(fields) >= 3 and fields[0] == "SWITCH":
            prev = fields[1].split("=", 1)[1] if "=" in fields[1] else "idle"
            curr = fields[2].split("=", 1)[1] if "=" in fields[2] else "idle"
            count = fields[3].split("=", 1)[1] if "=" in fields[3] else "0"
            self.last_switch.set(f"{prev} -> {curr}")
            self.switch_count.set(count)
            self.current_task.set(curr)
            self.switch_events.append({"time": now, "prev": prev, "curr": curr, "count": count})
            self.switch_events = self.switch_events[-20:]

        self.last_message_at = now
        self.feed.insert("", "end", values=(time.strftime("%H:%M:%S"), fields[0] if fields else "RAW", line))
        rows = self.feed.get_children()
        if len(rows) > 80:
            self.feed.delete(rows[0])

    def refresh_view(self) -> None:
        self.uptime_text.set(f"{int(time.monotonic() - self.started_at):,} s")
        if self.last_message_at:
            age = time.monotonic() - self.last_message_at
            if age > 2 and self.connection:
                self.rx_state.set("No data received")
        recent = [t for t, _ in self.tick_history if time.monotonic() - t < 5]
        self.rate_text.set(f"{len(recent) / 5:.1f} Hz")
        self.draw_tasks()
        self.draw_chart()
        self.after(250, self.refresh_view)

    def draw_tasks(self) -> None:
        for child in self.task_area.winfo_children():
            child.destroy()
        for name in sorted(self.tasks):
            task = self.tasks[name]
            age = time.monotonic() - task["seen"]
            color = GREEN if age < 1.5 else AMBER if age < 3 else RED
            row = tk.Frame(self.task_area, bg=PANEL, padx=14, pady=11)
            row.pack(fill="x", pady=3)
            tk.Label(row, text=name.upper(), bg=PANEL, fg=TEXT,
                     font=("Consolas", 11, "bold"), width=12, anchor="w").pack(side="left")
            tk.Label(row, text=task["state"], bg=PANEL, fg=color,
                     font=("Segoe UI", 9, "bold"), width=14, anchor="w").pack(side="left")
            tk.Label(row, text=f"tick {task['tick']:,}", bg=PANEL, fg=MUTED,
                     font=("Consolas", 10)).pack(side="right")

    def draw_chart(self) -> None:
        self.chart.delete("all")
        width = max(self.chart.winfo_width(), 300)
        height = 145
        self.chart.create_line(0, height - 22, width, height - 22, fill="#2b3b48")
        if len(self.tick_history) < 2:
            self.chart.create_text(18, 22, text="Waiting for TASK telemetry...", anchor="w", fill=MUTED)
            return
        values = [value for _, value in self.tick_history]
        low, high = min(values), max(values)
        span = max(high - low, 1)
        points = []
        for index, value in enumerate(values):
            x = 12 + index * (width - 24) / max(len(values) - 1, 1)
            y = 12 + (high - value) * (height - 42) / span
            points.extend((x, y))
        self.chart.create_line(*points, fill=CYAN, width=2, smooth=True)

    def quick_command(self, command: str) -> None:
        self.command_var.set(command)
        self.send_command()

    def apply_delay(self, task: str) -> None:
        try:
            delay_ms = int(self.delay_inputs[task].get())
        except ValueError:
            messagebox.showwarning("Invalid delay", "Enter a whole number of milliseconds.")
            return
        if not 1 <= delay_ms <= 60000:
            messagebox.showwarning("Invalid delay", "Delay must be between 1 and 60000 ms.")
            return
        self.quick_command(f"delay {task} {delay_ms}")

    def animate(self) -> None:
        self.animation_step = (self.animation_step + 1) % 24
        self.signal_canvas.delete("all")
        active = bool(self.connection) or self.demo_job is not None
        if active:
            radius = 5 + self.animation_step % 8
            self.signal_canvas.create_oval(12 - radius, 12 - radius,
                                           12 + radius, 12 + radius,
                                           outline=CYAN, width=1)
            self.signal_canvas.create_oval(8, 8, 16, 16, fill=CYAN, outline="")
        else:
            self.signal_canvas.create_oval(8, 8, 16, 16, fill=MUTED, outline="")
        self.after(90, self.animate)

    def set_link(self, connected: bool, label: str | None = None) -> None:
        text = label or ("CONNECTED" if connected else "DISCONNECTED")
        self.link_text.set(text)
        self.rx_state.set("Listening for telemetry" if connected else "Waiting for UART telemetry")
        self.connect_button.configure(text="DISCONNECT" if connected else "CONNECT")

    def send_command(self, event=None) -> None:
        cmd = self.command_var.get().strip()
        if not cmd:
            return
        self.command_var.set("")
        if not self.connection:
            self.handle_line("SYS,CMD,ERR,not connected")
            return
        try:
            self.connection.write((cmd + "\r\n").encode("utf-8"))
            self.feed.insert("", "end", values=(time.strftime("%H:%M:%S"), "CMD", cmd))
            rows = self.feed.get_children()
            if len(rows) > 80:
                self.feed.delete(rows[0])
        except (TypeError, OSError, ValueError) as exc:
            self.handle_line(f"SYS,CMD,ERR,{exc}")

    def close(self) -> None:
        if self.demo_job:
            self.after_cancel(self.demo_job)
        self.disconnect()
        self.destroy()


if __name__ == "__main__":
    UartMonitor().mainloop()
