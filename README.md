# tinyRTOS — Interrupt-Driven Real-Time Operating System (ARM Cortex-M4)

A lightweight, preemptive real-time operating system built from scratch for STM32F401 (ARM Cortex-M4), designed to explore low-level OS concepts including scheduling, context switching, and exception handling.

This project focuses on understanding how an RTOS works internally rather than relying on existing frameworks like FreeRTOS.

---

## ⚙️ Features

- Preemptive multitasking using SysTick timer
- Interrupt-driven scheduler (PendSV-based context switching)
- Fixed-priority, multi-queue round-robin scheduling
- Task isolation using Process Stack Pointer (PSP)
- Lightweight system call interface using SVC exceptions
- Basic task delay/blocking support

---

## 🧠 System Design Overview

tinyRTOS is built around ARM Cortex-M exception mechanisms:

- **SysTick** → triggers periodic scheduling
- **PendSV** → handles context switching
- **SVC** → provides controlled transitions between user tasks and kernel logic

The design cleanly separates:
- Scheduling logic (policy)
- Context switching (mechanism)
- Task execution (user space)

---

## 🧩 Core Concepts Implemented

### 🧵 Task Model
Each task is represented using a Task Control Block (TCB) containing:
- Stack pointer (PSP)
- Task state (Ready / Blocked / Delayed)
- Priority level

### ⏱ Scheduler
- Fixed-priority scheduling
- Round-robin within same priority level
- Ready, blocked, and delay queues

### 🔄 Context Switching
- Triggered via PendSV exception
- Full CPU state saved/restored during switch
- Uses PSP for task isolation

### 🧷 System Calls (SVC)
- Controlled kernel entry point
- Used for task delay and scheduler interaction

---

## 🧪 Debugging & Challenges

This project involved significantly more debugging than implementation.

### Key Issue: PendSV Stack Corruption

A critical bug caused random system crashes during context switching.

#### Root Cause:
PendSV handler was corrupting stack state due to incorrect assumptions about ARM exception entry behavior and compiler optimizations.

Specifically:
- Compiler optimizations were altering expected register state handling
- Stack frame assumptions during exception entry were incorrect
- Missing `volatile` semantics allowed unintended register handling behavior

#### Fix:
- Corrected exception entry/exit assumptions
- Introduced `volatile` where required to prevent compiler interference
- Revalidated stack frame layout during context switching

This bug revealed deeper insights into:
- ARM Cortex-M exception model
- Compiler + hardware interaction
- Real-world RTOS stability issues

---

## 🧠 Key Learnings

- How ARM Cortex-M handles exceptions internally
- Real implementation of context switching (beyond theory)
- Trade-offs in RTOS scheduling design
- Debugging low-level stack and register corruption issues
- Importance of compiler behavior in embedded systems

---

## 🛠️ Tech Stack

- C (bare-metal)
- ARM Cortex-M4 (STM32F401)
- CMSIS / startup-level programming

---

## 🚀 Future Improvements

- Priority inversion handling (mutex + priority inheritance)
- Memory allocator for dynamic tasks
- Tickless idle mode
- Basic IPC (message queues / semaphores)

---

## 📷 Hardware

- STM32F401 (ARM Cortex-M4)

---

## 📜 License

For educational purposes.