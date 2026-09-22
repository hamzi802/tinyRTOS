# tinyRTOS UART Monitor

A small desktop dashboard for the telemetry emitted by the STM32 tinyRTOS demo.

## Run

```powershell
py -m pip install -r monitor/requirements.txt
py monitor/uart_monitor_qt.py
```

The PyQt6 Mission Control dashboard opens at 115200 baud. It automatically prefers
FTDI adapters, shows task health and scheduler activity, and includes quick UART
commands. The original Tkinter monitor remains available as a lightweight fallback:

```powershell
py monitor/uart_monitor.py
```

Use **Demo Stream** in the fallback monitor to preview the dashboard without hardware.

Each discovered task has a delay control in the Qt dashboard. Enter a value in
milliseconds and select **APPLY**; the firmware updates that task's blink interval
immediately after its current delay finishes. The Tkinter fallback has the same
controls in its task-delay panel. The equivalent UART command is:

```text
delay task1 2000
```

Valid task names are `task1`, `task2`, and `task3`; delays may be 1 to 60000 ms.

## USB-to-TTL wiring

Use a 3.3 V adapter and connect:

- Adapter TX -> Black Pill PA10 / USART1_RX
- Adapter RX -> Black Pill PA9 / USART1_TX
- Adapter GND -> Black Pill GND

Do not connect the adapter 5 V pin to the 3.3 V UART pins. Flash the firmware with the existing ST-Link/SWD setup, or use your board's supported bootloader procedure.

The firmware sends records in this format:

```text
TASK,task3,1234,HEARTBEAT
```
