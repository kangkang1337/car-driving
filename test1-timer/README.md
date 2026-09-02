# TEST BY MYSELF

This experiment makes the QST car drive forward on a table and avoid falling from the edge by using the two infrared pair sensors.

## Hardware

- Left infrared sensor: GPIO14
- Right infrared sensor: GPIO13
- Motor control UART: UART2
- UART2 TX: GPIO11
- UART2 RX: GPIO12
- Motor UART baud rate: 115200

The tested sensor logic is:

```text
L=0, R=0: safe, the car is on the table
L=1 or R=1: edge detected, the car should stop and move back
```

## Build

Put this folder under the Hi3861 OpenHarmony app path:

```text
applications/sample/wifi-iot/app/2.0Timer/test
```

Add this module to the parent `BUILD.gn`:

```gn
features = [
  "2.0Timer/test:TableEdgeBrake",
]
```

## Behavior

1. The car starts with a short high-speed boost.
2. The car continues driving forward at cruise speed.
3. If either infrared sensor detects the table edge, the car stops immediately.
4. The car moves backward for a short time.
5. The car turns away from the detected edge.
6. The car continues driving forward and repeats the same process.

## Tunable Parameters

These values are defined at the top of `table_edge_brake.c`:

```c
#define BRAKE_TIME_MS 100
#define BACKWARD_TIME_MS 300
#define TURN_TIME_MS 450
#define START_SPEED 150
#define CRUISE_SPEED 120
#define START_BOOST_TIME_MS 300
#define BACKWARD_SPEED 150
#define TURN_SPEED 120
```

Adjust `TURN_TIME_MS` if the car turns too little or too much. Adjust `CRUISE_SPEED` if the car is still too fast near the edge.
