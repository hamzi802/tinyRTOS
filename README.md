

# MINI RTOS

1. First thing we need is to setup SysTick timer so that it triggers an interrupt every 1ms.
By default, when we do HAL_Config, it sets up the interrupt to trigger every 1ms.
So, no need to do that using the SysTick_Config(ticks) function from CMSIS.



# TODO
- implement waiting/block etc queues and update the rtoswakeffromblock function so that task is pushed into the blocked state or removed from one.
- Add rtos delay function that makes the function in block state for the duratioin of the delay.  
- Handle the schedluer so that it puts the tasks whole time delay has ended into the ready_queue.