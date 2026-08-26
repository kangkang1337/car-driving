# Hi3861 Hello World

This OpenHarmony Hi3861 sample creates two CMSIS-RTOS2 threads:

- `thread1`: prints task 1 status and `Hello World!` every 1 second.
- `thread2`: waits 1 second first, then prints task 2 status and `Hello QST!` every 3 seconds.

Put this directory under the Hi3861 app path, for example:

```text
applications/sample/wifi-iot/app/1.0Hello_world
```

Then add the module to the parent `BUILD.gn`:

```gn
features = [
  "1.0Hello_world:hello_world",
]
```

After flashing the firmware, open the serial assistant. It should receive:

```text
任务1正在运行!
Hello World!
任务2正在运行!
Hello QST!
```
