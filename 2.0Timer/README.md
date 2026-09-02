# Hi3861 Infrared Pair Timer

This OpenHarmony Hi3861 sample reads the two digital infrared pair sensors on the QST car board with a CMSIS software timer and reports the result through UART1.

## Hardware

- Left infrared pair sensor digital output: GPIO14
- Right infrared pair sensor digital output: GPIO13
- UART1 TX: GPIO0
- UART1 RX: GPIO1
- Serial assistant baud rate: 9600, 8 data bits, 1 stop bit, no parity

This board's actual direction is GPIO14 for the left infrared sensor and GPIO13 for the right infrared sensor. Its trace example treats HIGH as a detected signal, so this sample prints `success, signal detected` when either GPIO13 or GPIO14 is HIGH.

## Build

Put this directory under the Hi3861 app path, for example:

```text
applications/sample/wifi-iot/app/2.0Timer
```

Then add the module to the parent `BUILD.gn`:

```gn
features = [
  "2.0Timer:InfraredPairTimer",
]
```

After flashing the firmware, open the serial assistant. It should receive messages like:

```text
Infrared pair timer start.
GPIO14 is left IR, GPIO13 is right IR. HIGH means signal detected.
IR status: L=0 R=0 waiting, no signal
IR changed: L=1 R=0 success, signal detected
```
