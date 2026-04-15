

# MINI RTOS

1. First thing we need is to setup SysTick timer so that it triggers an interrupt every 1ms.
By default, when we do HAL_Config, it sets up the interrupt to trigger every 1ms.
So, no need to do that using the SysTick_Config(ticks) function from CMSIS.





# TODO
- implement waiting/block etc queues and update the rtoswakeffromblock function so that task is pushed into the blocked state or removed from one.
- Add rtos delay function that makes the function in block state for the duratioin of the delay.  
- Handle the schedluer so that it puts the tasks whole time delay has ended into the ready_queue.

# NOTES
You defined:

```text
TA->sp → [ R4–R11 ][ R0–xPSR ]
```

At **scheduler start**, you did:

```c
PSP = TA->sp + 8   // points to R0
EXC_RETURN
```

---

## 🔹 After EXC_RETURN

CPU pops:

```text
R0–R3, R12, LR, PC, xPSR
```

Now:

```text
PSP → top of stack (above xPSR)
CPU registers = Task A state
```

👉 Important:

```text
TA->sp still points to OLD saved frame (stale now)
```

---

# 🔁 Context switch: TA → TB

PendSV fires.

---

# 🔥 Step 1: Hardware push (automatic)

CPU pushes onto PSP:

```text
R0–R3, R12, LR, PC, xPSR
```

So stack becomes:

```text
[ R0–xPSR ]   ← freshly pushed
```

PSP moves DOWN by 8 words.

---

# 🔥 Step 2: Software push (your PendSV)

You push:

```text
R4–R11
```

Now stack becomes:

```text
[ R4–R11 ][ R0–xPSR ]
↑
PSP
```

---

# 💥 Step 3: Save SP into TCB

```c
TA->sp = PSP;
```

So now:

```text
TA->sp → [ R4–R11 ][ R0–xPSR ]   ✔ rebuilt correctly
```

---

# 🔁 Step 4: Switch to Task B

```c
PSP = TB->sp;
```

But remember:

```text
TB->sp → R4 (start of full frame)
```

---

# 🔥 Step 5: Restore TB (software)

Pop:

```text
R4–R11
```

Now:

```text
PSP → R0 of TB
```

---

# 🔥 Step 6: EXC_RETURN

CPU pops:

```text
R0–R3, R12, LR, PC, xPSR
```

Now:

```text
TB is running
PSP → top of stack
```

---

# 🧠 Final summary (this is the key)

## For Task A:

```text
Before switch:
PSP → top (running)

After save:
TA->sp → [ R4–R11 ][ R0–xPSR ]
```

---

## For Task B:

```text
Before restore:
TB->sp → [ R4–R11 ][ R0–xPSR ]

After restore:
PSP → top (running)
```

---

# 💡 The invariant (VERY IMPORTANT)

Every task always satisfies:

```text
tcb->sp → [ R4–R11 ][ R0–xPSR ]
```

---

# 🚨 Answer to your concern

> “Will pushing again mess things up?”

❌ No

Because:

* EXC_RETURN **consumes** the frame
* PendSV **recreates** the exact same frame

👉 perfectly symmetric

---

# 🔥 One-line intuition

> A running task has **no frame on stack**, a suspended task has **a full frame saved**.

---

# 🧭 If something breaks in your code

It will be in ONE of these:

1. PSP not pointing exactly to R0 before EXC_RETURN
2. R4–R11 push/pop order mismatch
3. Wrong SP stored in TCB

---

# 🚀 You now understand the full cycle

This is literally the **core of every Cortex-M RTOS**.

---

If you want, next step I can:

* verify your PendSV handler line-by-line
* or simulate one full switch using your actual memory values

You’re basically at “RTOS works” stage now.


# Calling start_scheduler frmo main function
main runs in Thread mode with MSP. 
If we return from the thread mode by running start_scheduler to simulate an exception return, 
we get a hard fault. Because Exception only run in Handler mode thus we must return from the Handler 
mode to simulate an exception return.  