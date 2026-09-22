"""tinyRTOS Mission Control, PyQt6 edition."""

from __future__ import annotations

import csv
import io
import random
import sys
import time
from dataclasses import dataclass

import serial
from serial.tools import list_ports
from PyQt6.QtCore import QObject, QPointF, QThread, QTimer, Qt, pyqtSignal
from PyQt6.QtGui import QColor, QFont, QPainter, QPainterPath, QPen
from PyQt6.QtWidgets import (
    QApplication,
    QComboBox,
    QFrame,
    QGridLayout,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QListWidget,
    QListWidgetItem,
    QMainWindow,
    QMessageBox,
    QPushButton,
    QSpinBox,
    QSizePolicy,
    QSplitter,
    QStatusBar,
    QVBoxLayout,
    QWidget,
)


BG = "#0e0f10"
SURFACE = "#171818"
SURFACE_2 = "#222323"
BORDER = "#303131"
TEXT = "#f1f1ed"
MUTED = "#858684"
CYAN = "#f3c94b"
BLUE = "#668cff"
GREEN = "#28d6a0"
AMBER = "#ff9257"
RED = "#ed5064"


@dataclass
class TaskInfo:
    tick: int
    state: str
    seen: float


class SerialWorker(QObject):
    line_received = pyqtSignal(str)
    connection_lost = pyqtSignal(str)
    finished = pyqtSignal()

    def __init__(self, port: str, baud: int) -> None:
        super().__init__()
        self.port = port
        self.baud = baud
        self.running = True
        self.serial = None

    def run(self) -> None:
        try:
            self.serial = serial.Serial(self.port, self.baud, timeout=0.25)
            while self.running:
                line = self.serial.readline()
                if line:
                    self.line_received.emit(line.decode("utf-8", errors="replace").strip())
        except (serial.SerialException, OSError) as error:
            if self.running:
                self.connection_lost.emit(str(error))
        finally:
            if self.serial:
                self.serial.close()
            self.finished.emit()

    def send(self, command: str) -> None:
        if self.serial and self.serial.is_open:
            self.serial.write((command + "\r\n").encode("utf-8"))

    def stop(self) -> None:
        self.running = False
        if self.serial and self.serial.is_open:
            self.serial.close()


class TimelineWidget(QWidget):
    def __init__(self) -> None:
        super().__init__()
        self.setMinimumHeight(170)
        self.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)
        self.events: list[tuple[float, str]] = []
        self.phase = 0

    def set_events(self, events: list[tuple[float, str]]) -> None:
        self.events = events[-70:]
        self.update()

    def pulse(self) -> None:
        self.phase = (self.phase + 1) % 40
        self.update()

    def paintEvent(self, event) -> None:
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        painter.fillRect(self.rect(), QColor(SURFACE))
        left, right = 24, self.width() - 24
        baseline = self.height() - 36
        painter.setPen(QPen(QColor(BORDER), 1))
        painter.drawLine(left, baseline, right, baseline)
        if not self.events:
            painter.setPen(QColor(MUTED))
            painter.setFont(QFont("Segoe UI", 10))
            painter.drawText(left, 34, "Waiting for scheduler telemetry...")
            return
        recent = self.events[-55:]
        spacing = (right - left) / max(len(recent) - 1, 1)
        path = QPainterPath()
        for index, (_, name) in enumerate(recent):
            x = left + index * spacing
            color = (CYAN, BLUE, AMBER)[sum(map(ord, name)) % 3]
            height = 45 + (sum(map(ord, name)) % 42)
            painter.setPen(QPen(QColor(color), 2))
            painter.drawLine(QPointF(x, baseline), QPointF(x, baseline - height))
            painter.setBrush(QColor(color))
            painter.drawEllipse(QPointF(x - 4, baseline - height - 4), 4, 4)
            point = QPointF(x, baseline - height)
            if index == 0:
                path.moveTo(point)
            else:
                path.lineTo(point)
        painter.setPen(QPen(QColor(CYAN), 1.5))
        painter.drawPath(path)
        scan_x = left + ((self.phase / 40) * (right - left))
        painter.setPen(QPen(QColor(CYAN), 1))
        painter.drawLine(QPointF(scan_x, 18), QPointF(scan_x, baseline + 1))
        painter.setBrush(QColor(CYAN))
        painter.drawEllipse(QPointF(scan_x - 3, 18), 3, 3)
        painter.setPen(QColor(MUTED))
        painter.setFont(QFont("Segoe UI", 8))
        painter.drawText(left, self.height() - 12, "older")
        painter.drawText(right - 24, self.height() - 12, "now")


class RingWidget(QWidget):
    def __init__(self, title: str, unit: str, color: str) -> None:
        super().__init__()
        self.title = title
        self.color = QColor(color)
        self.unit = unit
        self.value = 0.0
        self.display = "--"
        self.setMinimumSize(112, 112)

    def set_value(self, value: float, display: str, unit: str | None = None) -> None:
        self.value = max(0.0, min(1.0, value))
        self.display = display
        if unit is not None:
            self.unit = unit
        self.update()

    def paintEvent(self, event) -> None:
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        center = self.rect().center()
        radius = min(self.width(), self.height()) * 0.31
        pen = QPen(QColor(BORDER), 7)
        pen.setCapStyle(Qt.PenCapStyle.RoundCap)
        painter.setPen(pen)
        painter.drawArc(int(center.x() - radius), int(center.y() - radius),
                        int(radius * 2), int(radius * 2), 0, 360 * 16)
        pen.setColor(self.color)
        pen.setWidth(7)
        painter.setPen(pen)
        painter.drawArc(int(center.x() - radius), int(center.y() - radius),
                        int(radius * 2), int(radius * 2), 90 * 16,
                        int(-360 * 16 * self.value))
        painter.setPen(QColor(TEXT))
        painter.setFont(QFont("Segoe UI", 16, QFont.Weight.Bold))
        painter.drawText(0, int(center.y() - 19), self.width(), 25,
                 Qt.AlignmentFlag.AlignCenter, self.display)
        painter.setPen(QColor(MUTED))
        painter.setFont(QFont("Segoe UI", 7, QFont.Weight.Bold))
        painter.drawText(0, int(center.y() + 4), self.width(), 14,
                 Qt.AlignmentFlag.AlignCenter, self.unit.upper())
        painter.setPen(QColor(MUTED))
        painter.setFont(QFont("Segoe UI", 8, QFont.Weight.Bold))
        painter.drawText(0, self.height() - 6, self.width(), 18,
                         Qt.AlignmentFlag.AlignCenter, self.title.upper())


class TaskCard(QFrame):
    def __init__(self, name: str, send_delay) -> None:
        super().__init__()
        self.name = name
        self.setObjectName("taskCard")
        self.setMinimumHeight(66)
        layout = QHBoxLayout(self)
        layout.setContentsMargins(16, 10, 16, 10)
        identity = QVBoxLayout()
        self.name_label = QLabel(name.upper())
        self.name_label.setObjectName("taskName")
        self.state_label = QLabel("WAITING")
        self.state_label.setObjectName("taskState")
        identity.addWidget(self.name_label)
        identity.addWidget(self.state_label)
        layout.addLayout(identity)
        layout.addStretch()
        self.tick_label = QLabel("tick --")
        self.tick_label.setObjectName("taskTick")
        layout.addWidget(self.tick_label)
        self.delay_input = QSpinBox()
        self.delay_input.setRange(1, 60000)
        self.delay_input.setValue({"task1": 300, "task2": 600, "task3": 100}.get(name, 1000))
        self.delay_input.setSuffix(" ms")
        self.delay_input.setToolTip("Delay before the next task blink")
        layout.addWidget(self.delay_input)
        apply_button = QPushButton("APPLY")
        apply_button.setObjectName("quickButton")
        apply_button.clicked.connect(lambda: send_delay(self.name, self.delay_input.value()))
        layout.addWidget(apply_button)

    def update_task(self, task: TaskInfo) -> None:
        age = time.monotonic() - task.seen
        color = GREEN if age < 1.5 else AMBER if age < 3 else RED
        self.state_label.setText("LIVE" if age < 1.5 else "STALE")
        self.state_label.setStyleSheet(f"color: {color};")
        self.tick_label.setText(f"tick {task.tick:,}")


class MissionControl(QMainWindow):
    def __init__(self) -> None:
        super().__init__()
        self.setWindowTitle("tinyRTOS | Mission Control")
        self.resize(1320, 850)
        self.setMinimumSize(1050, 700)
        self.worker = None
        self.thread = None
        self.started = time.monotonic()
        self.last_message = None
        self.messages = 0
        self.tasks: dict[str, TaskInfo] = {}
        self.events: list[tuple[float, str]] = []
        self.tick_history: list[tuple[float, int]] = []
        self.demo_timer = None
        self.demo_tick = 0
        self.view_paused = False
        self.auto_scroll = True
        self.task_cards: dict[str, TaskCard] = {}
        self.build_ui()
        self.refresh_ports()
        self.refresh_timer = QTimer(self)
        self.refresh_timer.timeout.connect(self.refresh_view)
        self.refresh_timer.start(250)
        self.animation_timer = QTimer(self)
        self.animation_timer.timeout.connect(self.animate)
        self.animation_timer.start(80)

    def build_ui(self) -> None:
        root = QWidget()
        root.setObjectName("root")
        self.setCentralWidget(root)
        self.setStatusBar(QStatusBar())
        outer = QHBoxLayout(root)
        outer.setContentsMargins(0, 0, 0, 0)
        outer.addWidget(self.sidebar())

        content = QWidget()
        content_layout = QVBoxLayout(content)
        content_layout.setContentsMargins(30, 25, 30, 26)
        content_layout.setSpacing(16)
        content_layout.addWidget(self.header())
        content_layout.addWidget(self.stats_row())
        content_layout.addWidget(self.workspace_strip())
        content_layout.addWidget(self.body(), 1)
        outer.addWidget(content, 1)

    def workspace_strip(self) -> QWidget:
        panel = QFrame()
        panel.setObjectName("workspaceStrip")
        layout = QHBoxLayout(panel)
        layout.setContentsMargins(16, 10, 16, 10)
        copy = QVBoxLayout()
        self.workspace_title = QLabel("MISSION CONTROL / LIVE OVERVIEW")
        self.workspace_title.setObjectName("workspaceTitle")
        self.workspace_detail = QLabel("Select a workspace from the left rail to inspect a focused view.")
        self.workspace_detail.setObjectName("workspaceDetail")
        copy.addWidget(self.workspace_title)
        copy.addWidget(self.workspace_detail)
        layout.addLayout(copy)
        layout.addStretch()
        self.workspace_action = QPushButton("LIVE")
        self.workspace_action.setObjectName("workspaceAction")
        self.workspace_action.setEnabled(False)
        layout.addWidget(self.workspace_action)
        return panel

    def sidebar(self) -> QWidget:
        panel = QFrame()
        panel.setObjectName("sidebar")
        panel.setFixedWidth(235)
        layout = QVBoxLayout(panel)
        layout.setContentsMargins(20, 28, 20, 22)
        brand = QLabel('<b>tiny</b><span style="color:#56e3da">RTOS</span>')
        brand.setObjectName("brand")
        layout.addWidget(brand)
        subtitle = QLabel("EMBEDDED SYSTEMS LAB")
        subtitle.setObjectName("eyebrow")
        layout.addWidget(subtitle)
        layout.addSpacing(25)
        self.nav_buttons = {}
        for icon, text in (("◈", "Mission control"), ("◌", "Task scheduler"),
                           ("⌁", "Serial console"), ("◇", "Diagnostics")):
            item = QPushButton(f"  {icon}    {text}")
            item.setObjectName("navActive" if text == "Mission control" else "navItem")
            item.setCheckable(True)
            item.setChecked(text == "Mission control")
            item.clicked.connect(lambda checked=False, name=text: self.navigate(name))
            self.nav_buttons[text] = item
            layout.addWidget(item)
        layout.addStretch()
        transport = QLabel("TRANSPORT")
        transport.setObjectName("eyebrow")
        layout.addWidget(transport)
        self.port_combo = QComboBox()
        self.port_combo.setObjectName("darkCombo")
        layout.addWidget(self.port_combo)
        self.baud_combo = QComboBox()
        self.baud_combo.addItems(["9600", "115200", "230400"])
        self.baud_combo.setCurrentText("115200")
        self.baud_combo.setObjectName("darkCombo")
        layout.addWidget(self.baud_combo)
        self.connect_button = QPushButton("CONNECT DEVICE")
        self.connect_button.setObjectName("primaryButton")
        self.connect_button.clicked.connect(self.toggle_connection)
        layout.addWidget(self.connect_button)
        refresh = QPushButton("Refresh ports")
        refresh.setObjectName("quietButton")
        refresh.clicked.connect(self.refresh_ports)
        layout.addWidget(refresh)
        tools = QLabel("TOOLS")
        tools.setObjectName("eyebrow")
        layout.addWidget(tools)
        self.pause_button = QPushButton("Pause live view")
        self.pause_button.setObjectName("quietButton")
        self.pause_button.clicked.connect(self.toggle_pause)
        layout.addWidget(self.pause_button)
        clear = QPushButton("Clear session")
        clear.setObjectName("quietButton")
        clear.clicked.connect(self.clear_session)
        layout.addWidget(clear)
        demo = QPushButton("Start demo stream")
        demo.setObjectName("quietButton")
        demo.clicked.connect(self.toggle_demo)
        self.demo_button = demo
        layout.addWidget(demo)
        ping = QPushButton("Send ping command")
        ping.setObjectName("quietButton")
        ping.clicked.connect(lambda: self.quick_command("status"))
        layout.addWidget(ping)
        layout.addWidget(QLabel("USART1  /  115200 8N1"))
        return panel

    def navigate(self, section: str) -> None:
        for name, button in self.nav_buttons.items():
            button.setChecked(name == section)
            button.setObjectName("navActive" if name == section else "navItem")
            button.style().unpolish(button)
            button.style().polish(button)
        self.page_title.setText(section)
        if section == "Task scheduler":
            self.timeline.setFocus()
            self.page_subtitle.setText("Priority queues, heartbeat timing, and task execution history")
            self.workspace_title.setText("TASK SCHEDULER / EXECUTION MAP")
            self.workspace_detail.setText("Inspect task freshness, scheduling rhythm, and priority activity in real time.")
            self.workspace_action.setText("TIMELINE ACTIVE")
            self.workspace_action.setStyleSheet(f"color: {CYAN};")
            self.timeline.setStyleSheet(f"border: 1px solid {CYAN}; border-radius: 7px;")
            self.feed.setStyleSheet("")
            message = "Scheduler activity selected"
        elif section == "Serial console":
            self.feed.setFocus()
            self.page_subtitle.setText("Live USART1 records and device command traffic")
            self.workspace_title.setText("SERIAL CONSOLE / DEVICE STREAM")
            self.workspace_detail.setText("Read incoming telemetry and send commands to the STM32 over USART1.")
            self.workspace_action.setText("UART ACTIVE")
            self.workspace_action.setStyleSheet(f"color: {GREEN};")
            self.feed.setStyleSheet(f"border: 1px solid {CYAN}; border-radius: 7px;")
            self.timeline.setStyleSheet("")
            message = "Serial feed selected"
        elif section == "Diagnostics":
            self.page_subtitle.setText("Connection health, task freshness, and runtime counters")
            self.workspace_title.setText("DIAGNOSTICS / SYSTEM HEALTH")
            self.workspace_detail.setText("Review link state, task count, message volume, and recent runtime health.")
            self.workspace_action.setText("HEALTH CHECK")
            self.workspace_action.setStyleSheet(f"color: {AMBER};")
            self.timeline.setStyleSheet("")
            self.feed.setStyleSheet(f"border: 1px solid {AMBER}; border-radius: 7px;")
            self.add_feed("INFO", f"UART={'ONLINE' if self.thread else 'OFFLINE'} | tasks={len(self.tasks)} | messages={self.messages}")
            message = f"UART: {'online' if self.thread else 'offline'}  |  tasks: {len(self.tasks)}  |  messages: {self.messages}"
        else:
            self.page_subtitle.setText("Live observability for your Cortex-M4 scheduler")
            self.workspace_title.setText("MISSION CONTROL / LIVE OVERVIEW")
            self.workspace_detail.setText("All scheduler, UART, and task signals in one operational view.")
            self.workspace_action.setText("LIVE")
            self.workspace_action.setStyleSheet(f"color: {CYAN};")
            self.timeline.setStyleSheet("")
            self.feed.setStyleSheet("")
            message = "Mission control selected"
        self.statusBar().showMessage(message, 4000)

    def header(self) -> QWidget:
        row = QWidget()
        layout = QHBoxLayout(row)
        layout.setContentsMargins(0, 0, 0, 0)
        title_box = QVBoxLayout()
        title = QLabel("Mission control")
        title.setObjectName("pageTitle")
        self.page_title = title
        title_box.addWidget(title)
        subtitle = QLabel("Live observability for your Cortex-M4 scheduler")
        self.page_subtitle = subtitle
        title_box.addWidget(subtitle)
        layout.addLayout(title_box)
        layout.addStretch()
        status = QFrame()
        status.setObjectName("statusPanel")
        status_layout = QHBoxLayout(status)
        status_layout.setContentsMargins(14, 9, 14, 9)
        self.signal = QLabel("●")
        self.signal.setObjectName("signal")
        status_layout.addWidget(self.signal)
        self.link_label = QLabel("OFFLINE")
        self.link_label.setObjectName("onlineLabel")
        status_layout.addWidget(self.link_label)
        self.rx_label = QLabel("Waiting for telemetry")
        status_layout.addWidget(self.rx_label)
        layout.addWidget(status)
        return row

    def stats_row(self) -> QWidget:
        row = QWidget()
        layout = QHBoxLayout(row)
        layout.setContentsMargins(0, 0, 0, 0)
        rings = QFrame()
        rings.setObjectName("gaugePanel")
        ring_layout = QHBoxLayout(rings)
        ring_layout.setContentsMargins(8, 4, 8, 4)
        self.rings = {
            "rx": RingWidget("RX", "messages", CYAN),
            "tasks": RingWidget("ACTIVE", "tasks", RED),
            "rate": RingWidget("RATE", "hertz", GREEN),
            "link": RingWidget("LINK", "state", BLUE),
        }
        for ring in self.rings.values():
            ring_layout.addWidget(ring)
        layout.addWidget(rings, 2)

        metrics = QFrame()
        metrics.setObjectName("metricPanel")
        metric_layout = QGridLayout(metrics)
        metric_layout.setContentsMargins(16, 8, 16, 8)
        metric_layout.setHorizontalSpacing(28)
        metric_layout.setVerticalSpacing(4)
        self.stat_labels = {}
        for index, (title, key, color) in enumerate((("RUNTIME", "runtime", BLUE),
                                                      ("MESSAGES", "messages", CYAN),
                                                      ("LAST TASK", "task", AMBER),
                                                      ("TICK RATE", "rate", GREEN))):
            card = QWidget()
            card_layout = QVBoxLayout(card)
            card_layout.setContentsMargins(0, 0, 0, 0)
            cap = QLabel(title)
            cap.setObjectName("caption")
            value = QLabel("--")
            value.setObjectName("metricValue")
            value.setStyleSheet(f"color: {color};")
            card_layout.addWidget(cap)
            card_layout.addWidget(value)
            self.stat_labels[key] = value
            metric_layout.addWidget(card, index // 2, index % 2)
        layout.addWidget(metrics, 1)
        return row

    def body(self) -> QWidget:
        splitter = QSplitter(Qt.Orientation.Horizontal)
        splitter.setChildrenCollapsible(False)
        left = QWidget()
        left_layout = QVBoxLayout(left)
        left_layout.setContentsMargins(0, 0, 8, 0)
        left_layout.addWidget(self.section_label("SCHEDULER ACTIVITY", "live task execution"))
        self.timeline = TimelineWidget()
        self.timeline.setFocusPolicy(Qt.FocusPolicy.StrongFocus)
        left_layout.addWidget(self.timeline)
        left_layout.addWidget(self.section_label("TASKS", "heartbeat health"))
        self.task_list = QVBoxLayout()
        self.task_list.setSpacing(7)
        left_layout.addLayout(self.task_list)
        left_layout.addStretch()

        right = QWidget()
        right_layout = QVBoxLayout(right)
        right_layout.setContentsMargins(8, 0, 0, 0)
        right_layout.addWidget(self.section_label("SERIAL FEED", "raw device events"))
        self.feed = QListWidget()
        self.feed.setObjectName("feed")
        right_layout.addWidget(self.feed, 1)
        right_layout.addWidget(self.command_panel())
        splitter.addWidget(left)
        splitter.addWidget(right)
        splitter.setSizes([620, 460])
        return splitter

    def section_label(self, title: str, subtitle: str) -> QWidget:
        row = QWidget()
        layout = QHBoxLayout(row)
        layout.setContentsMargins(0, 0, 0, 0)
        heading = QLabel(title)
        heading.setObjectName("sectionTitle")
        layout.addWidget(heading)
        hint = QLabel(subtitle)
        hint.setObjectName("sectionHint")
        layout.addWidget(hint)
        layout.addStretch()
        return row

    def command_panel(self) -> QWidget:
        panel = QFrame()
        panel.setObjectName("commandPanel")
        layout = QVBoxLayout(panel)
        layout.setContentsMargins(12, 10, 12, 10)
        label = QLabel("COMMAND CENTER")
        label.setObjectName("caption")
        layout.addWidget(label)
        row = QHBoxLayout()
        self.command = QLineEdit()
        self.command.setPlaceholderText("Type a firmware command...")
        self.command.returnPressed.connect(self.send_command)
        row.addWidget(self.command)
        send = QPushButton("SEND")
        send.setObjectName("sendButton")
        send.clicked.connect(self.send_command)
        row.addWidget(send)
        layout.addLayout(row)
        quick = QHBoxLayout()
        for text in ("status", "tasklist", "led on", "toggle"):
            button = QPushButton(text)
            button.setObjectName("quickButton")
            button.clicked.connect(lambda checked=False, value=text: self.quick_command(value))
            quick.addWidget(button)
        layout.addLayout(quick)
        return panel

    def refresh_ports(self) -> None:
        ports = list(list_ports.comports())
        self.port_combo.clear()
        for port in ports:
            self.port_combo.addItem(f"{port.device}  ·  {port.description}", port.device)
        preferred = next((index for index, port in enumerate(ports)
                          if "FTDI" in (port.description or "").upper()), 0)
        if ports:
            self.port_combo.setCurrentIndex(preferred)

    def toggle_connection(self) -> None:
        if self.thread and self.thread.isRunning():
            self.disconnect_device()
            return
        port = self.port_combo.currentData()
        if not port:
            QMessageBox.warning(self, "No UART device", "Connect the FTDI adapter and refresh ports first.")
            return
        self.thread = QThread(self)
        self.worker = SerialWorker(port, int(self.baud_combo.currentText()))
        self.worker.moveToThread(self.thread)
        self.thread.started.connect(self.worker.run)
        self.worker.line_received.connect(self.handle_line)
        self.worker.connection_lost.connect(self.connection_error)
        self.worker.finished.connect(self.thread.quit)
        self.thread.finished.connect(self.thread_deleted)
        self.thread.start()
        self.connect_button.setText("DISCONNECT")
        self.link_label.setText("ONLINE")
        self.rx_label.setText("Listening for telemetry")

    def disconnect_device(self) -> None:
        if self.worker:
            self.worker.stop()
        if self.thread:
            self.thread.quit()
            self.thread.wait(1500)
        self.thread = None
        self.worker = None
        self.connect_button.setText("CONNECT DEVICE")
        self.link_label.setText("OFFLINE")
        self.rx_label.setText("Waiting for telemetry")

    def thread_deleted(self) -> None:
        if self.thread and not self.thread.isRunning():
            self.thread.deleteLater()

    def connection_error(self, message: str) -> None:
        self.rx_label.setText("UART error")
        self.add_feed("ERR", message)
        self.disconnect_device()

    def handle_line(self, line: str) -> None:
        now = time.monotonic()
        self.messages += 1
        self.last_message = now
        if self.view_paused:
            return
        self.stat_labels["messages"].setText(f"{self.messages:,}")
        self.rx_label.setText("Receiving telemetry")
        fields = []
        try:
            fields = next(csv.reader(io.StringIO(line)))
        except (csv.Error, StopIteration):
            pass
        kind = fields[0] if fields else "RAW"
        if len(fields) >= 4 and kind == "TASK":
            try:
                tick = int(fields[2])
            except ValueError:
                tick = 0
            name = fields[1]
            self.tasks[name] = TaskInfo(tick, fields[3], now)
            self.events.append((now, name))
            self.tick_history.append((now, tick))
            self.tick_history = self.tick_history[-80:]
            self.stat_labels["task"].setText(name)
        self.add_feed(kind, line)

    def add_feed(self, kind: str, text: str) -> None:
        item = QListWidgetItem()
        item.setText(f"{time.strftime('%H:%M:%S')}   {kind:<7}  {text}")
        item.setForeground(QColor(TEXT))
        self.feed.addItem(item)
        while self.feed.count() > 100:
            self.feed.takeItem(0)
        if self.auto_scroll:
            self.feed.scrollToBottom()

    def send_command(self) -> None:
        command = self.command.text().strip()
        if not command:
            return
        self.command.clear()
        if self.worker:
            self.worker.send(command)
            self.add_feed("TX", command)
        else:
            self.add_feed("ERR", "not connected")

    def quick_command(self, command: str) -> None:
        self.command.setText(command)
        self.send_command()

    def set_task_delay(self, name: str, delay_ms: int) -> None:
        self.quick_command(f"delay {name} {delay_ms}")

    def toggle_pause(self) -> None:
        self.view_paused = not self.view_paused
        self.pause_button.setText("Resume live view" if self.view_paused else "Pause live view")
        self.rx_label.setText("View paused" if self.view_paused else "Receiving telemetry")
        self.statusBar().showMessage("Live rendering paused" if self.view_paused else "Live rendering resumed", 3000)

    def clear_session(self) -> None:
        self.messages = 0
        self.last_message = None
        self.tasks.clear()
        self.events.clear()
        self.tick_history.clear()
        self.feed.clear()
        for card in self.task_cards.values():
            card.deleteLater()
        self.task_cards.clear()
        self.stat_labels["messages"].setText("0")
        self.stat_labels["task"].setText("--")
        self.rx_label.setText("Session cleared")
        self.statusBar().showMessage("Telemetry session cleared", 3000)

    def toggle_demo(self) -> None:
        if self.demo_timer:
            self.demo_timer.stop()
            self.demo_timer = None
            self.demo_button.setText("Start demo stream")
            self.statusBar().showMessage("Demo stream stopped", 3000)
            return
        self.demo_timer = QTimer(self)
        self.demo_timer.timeout.connect(self.demo_line)
        self.demo_timer.start(220)
        self.demo_button.setText("Stop demo stream")
        self.statusBar().showMessage("Demo stream started", 3000)
        self.demo_line()

    def demo_line(self) -> None:
        self.demo_tick += random.randint(70, 140)
        name = random.choice(("task1", "task2", "task3"))
        self.handle_line(f"TASK,{name},{self.demo_tick},HEARTBEAT")

    def refresh_view(self) -> None:
        now = time.monotonic()
        self.stat_labels["runtime"].setText(f"{int(now - self.started):,} s")
        self.stat_labels["messages"].setText(f"{self.messages:,}")
        recent = [stamp for stamp, _ in self.tick_history if now - stamp < 5]
        rate = len(recent) / 5
        self.stat_labels["rate"].setText(f"{rate:.1f} Hz")
        self.rings["rx"].set_value(min(self.messages / 160, 1), str(self.messages))
        self.rings["tasks"].set_value(min(len(self.tasks) / 4, 1), str(len(self.tasks)))
        self.rings["rate"].set_value(min(rate / 8, 1), f"{rate:.1f}")
        self.rings["link"].set_value(1 if self.thread and self.thread.isRunning() else 0, "ON" if self.thread else "OFF")
        for name, task in self.tasks.items():
            if name not in self.task_cards:
                card = TaskCard(name, self.set_task_delay)
                self.task_cards[name] = card
                self.task_list.addWidget(card)
            self.task_cards[name].update_task(task)
        self.timeline.set_events(self.events)
        if self.last_message and now - self.last_message > 2 and self.thread:
            self.rx_label.setText("No data received")
        for name, task in self.tasks.items():
            if name not in self.task_cards:
                card = TaskCard(name, self.set_task_delay)
                self.task_cards[name] = card
                self.task_list.addWidget(card)
            self.task_cards[name].update_task(task)
        self.timeline.set_events(self.events)

    def animate(self) -> None:
        active = bool(self.thread and self.thread.isRunning())
        if active:
            size = 14 + (self.timeline.phase % 5)
            self.signal.setStyleSheet(f"color: {CYAN}; font-size: {size}pt;")
        else:
            self.signal.setStyleSheet(f"color: {MUTED}; font-size: 14pt;")
        self.timeline.pulse()

    def closeEvent(self, event) -> None:
        if self.demo_timer:
            self.demo_timer.stop()
        self.disconnect_device()
        event.accept()


STYLE = f"""
QWidget {{ background: {BG}; color: {TEXT}; font-family: 'Segoe UI'; font-size: 10pt; }}
QLabel {{ background: transparent; border: 0; }}
#sidebar {{ background: {SURFACE}; border-right: 1px solid {BORDER}; }}
#root {{ background: {BG}; }}
#brand {{ font-size: 25pt; font-weight: 700; }}
#eyebrow, #caption {{ color: {MUTED}; font-size: 8pt; font-weight: 700; letter-spacing: 1px; }}
#navActive, #navItem {{ padding: 11px 8px; border-radius: 5px; text-align: left; }}
#navActive {{ background: #303b25; color: {CYAN}; font-weight: 700; }}
#navItem {{ color: {MUTED}; background: transparent; }}
#navItem:hover {{ background: {SURFACE_2}; color: {TEXT}; }}
#navActive:hover {{ background: #3a482b; }}
#qt_statusbar {{ background: {SURFACE}; color: {MUTED}; border-top: 1px solid {BORDER}; }}
#pageTitle {{ font-size: 26pt; font-weight: 700; }}
#statusPanel, #gaugePanel, #metricPanel, #commandPanel, #taskCard {{ background: {SURFACE}; border: 1px solid {BORDER}; border-radius: 7px; }}
#onlineLabel {{ color: {GREEN}; font-weight: 700; }}
#signal {{ color: {MUTED}; font-size: 18px; }}
#metricValue {{ font-family: Consolas; font-size: 17pt; font-weight: 700; }}
#sectionTitle {{ font-weight: 700; }}
#sectionHint {{ color: {MUTED}; font-size: 9pt; }}
#taskName {{ font-family: Consolas; font-weight: 700; }}
#taskState {{ font-size: 8pt; font-weight: 700; }}
#taskTick {{ color: {MUTED}; font-family: Consolas; }}
#feed {{ background: {SURFACE}; border: 1px solid {BORDER}; border-radius: 7px; padding: 8px; font-family: Consolas; font-size: 9pt; }}
QLineEdit, #darkCombo {{ background: {SURFACE_2}; border: 1px solid {BORDER}; border-radius: 5px; padding: 8px; color: {TEXT}; }}
QComboBox QAbstractItemView {{ background: {SURFACE_2}; color: {TEXT}; selection-background-color: #294657; }}
QPushButton {{ border: 0; border-radius: 5px; padding: 8px 12px; }}
#primaryButton {{ background: {CYAN}; color: #071316; font-weight: 700; }}
#sendButton {{ background: {GREEN}; color: #07180e; font-weight: 700; }}
#quietButton, #quickButton {{ background: {SURFACE_2}; color: {MUTED}; }}
#quietButton:hover, #quickButton:hover {{ background: {BORDER}; color: {TEXT}; }}
"""


def main() -> int:
    app = QApplication(sys.argv)
    app.setStyle("Fusion")
    app.setStyleSheet(STYLE)
    window = MissionControl()
    window.show()
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
