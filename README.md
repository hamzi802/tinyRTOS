

# MINI RTOS

1. First thing we need is to setup SysTick timer so that it triggers an interrupt every 1ms.
By default, when we do HAL_Config, it sets up the interrupt to trigger every 1ms.
So, no need to do that using the SysTick_Config(ticks) function from CMSIS.


