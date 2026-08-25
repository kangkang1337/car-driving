# car-driving
summer school from 8.24 to 9.6

8.25
modifying the LED effect after receiving the UART command：
1. Added a dual-side running light effect————LR_runingled(); This function updates both the left and right WS2812 LED data buffers and refreshes both LED strips in each frame.
2. Added a blade-style LED effect————Instead of simply lighting one LED at a time, this effect uses a bright main LED and dimmer LEDs as a tail, making the LEDs look like a rotating blade.
