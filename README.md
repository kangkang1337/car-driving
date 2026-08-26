# car-driving
summer school from 8.24 to 9.6

# 8.26
# Hi3861 OpenHarmony Practice Notes

This folder contains several small OpenHarmony examples for the Hi3861 board.

## Projects

### 1.0Hello_world

This project contains serial output examples:

- `hello_world.c`: creates two CMSIS-RTOS2 threads. `thread1` prints `Hello World!` every second. `thread2` starts one second later and prints `Hello QST!` every three seconds.
- `square_loader.c`: prints a square-style loading animation every 100 ms.
- `progress_bar.c`: prints a progress bar with a spinner every 100 ms.
- `beijing_clock.c`: prints a UTC+8 software clock once per second. The clock starts from `08:00:00` after reset because no RTC or network time source is used.

Only one file should normally be selected in `BUILD.gn` at a time, because each file has its own `APP_FEATURE_INIT(...)` entry.

Example:

```gn
static_library("hello_world") {
  sources = [
    "progress_bar.c",
  ]

  include_dirs = [
    "//utils/native/lite/include",
    "//kernel/liteos_m/components/cmsis/2.0",
  ]
}
```

### 3.0SG90

This project controls an SG90 servo connected to GPIO2.

`SG90.c` generates a 20 ms servo control pulse:

- `500 us` high level: left side
- `1500 us` high level: middle position
- `2500 us` high level: right side

The current program creates one `ServoSweep` thread. It moves the servo smoothly from the left side to the right side, then smoothly back to the left side, and repeats forever.

Speed can be tuned with:

```c
#define SG90_DUTY_STEP_US 20
#define SG90_PULSE_REPEAT 2
```

Increase `SG90_DUTY_STEP_US` to move faster. Increase `SG90_PULSE_REPEAT` to move slower and hold each step longer.

## App BUILD.gn

The parent file `applications/sample/wifi-iot/app/BUILD.gn` must reference the real folder and target name.

Examples:

```gn
features = [
  "1.0_Hello_World:hello_world",
  "2.0_SG90:sg90",
]
```

The folder name, target name, and source file name must match exactly. Linux is case-sensitive, so `SG90.c` and `sg90.c` are different files.

## Build

Build in the OpenHarmony source tree:

```bash
python build.py wifiiot
```

If the build succeeds, flash the generated Hi3861 firmware with HiBurn. After flashing, close HiBurn or disconnect its serial connection, open the serial assistant, and press `RESET` to start the program.

## Common Issues

- `Unable to load ".../BUILD.gn"`: the parent `BUILD.gn` points to a folder that does not exist, or the folder name contains an unexpected character.
- `Unresolved dependencies`: the target name in the child `BUILD.gn` does not match the parent feature label.
- `fatal error: cmsis_os2.h: No such file or directory`: add the CMSIS include path in the child `BUILD.gn`.
- `undefined reference to fflush`: do not use `fflush(stdout)` on this Hi3861 LiteOS build.
- Chinese text appears as garbled characters in the serial assistant: switch the serial assistant to UTF-8, or print ASCII text only.
- HiBurn shows `Wait connect success flag (hisilicon) overtime`: close other serial tools, select the correct COM port, hold `BOOT`, click burn/connect, press `RESET`, then release `BOOT` after downloading starts. Also check that the board switch is set back to Hi3861.
