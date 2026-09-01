# combine
some personal test based on both Hi3861 and STM32

# test1

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



# test2-sg90 obstacle avoidance car

This project separates the Hi3861 decision/sensor code and the STM32 motor-driver code.

## Directory layout

- `Hi3861/`: HC-SR04 + SG90 obstacle avoidance logic. It sends motor commands to STM32 through UART2.
- `STM32/`: STM32F103 Keil project based on the existing QST car template. It receives UART frames and drives the L9110S motor PWM.

## Wiring used by the code

Hi3861:

- SG90 signal: GPIO2
- HC-SR04 Trig: GPIO7
- HC-SR04 Echo: GPIO8
- UART2 TX to STM32 RX: GPIO11
- UART2 RX from STM32 TX: GPIO12
- Baud rate: 115200, 8N1

STM32:

- USART1 RX/TX: PA10/PA9
- USART2 RX/TX: PA3/PA2
- Left motor PWM: PB7 / TIM4_CH2
- Left motor direction: PB14
- Right motor PWM: PB6 / TIM4_CH1
- Right motor direction: PB13

## Motor UART frame

Hi3861 sends a 6-byte frame:

```text
0xFC, left_dir, left_speed, right_dir, right_speed, 0xFD
```

- `dir = 0`: forward
- `dir = 1`: reverse
- `speed`: 0 to 150

STM32 multiplies `speed` by 20 and writes the result to the TIM4 PWM channels.

## Behavior

The car moves forward by default. When the front HC-SR04 distance is valid and less than or equal to 20 cm, the car stops, keeps the SG90 ultrasonic head centered, spins in place, and checks the front distance during the turn. It continues forward only after the front distance is greater than 20 cm or the maximum turn time is reached. If the obstacle is still within 20 cm after the turn, it backs up briefly before trying again.

# 10.0SUM smart car integration

Run-time startup text must be:

```text
10.0SUM project start.
Task 1 running: car safety: IR edge protection + ultrasonic obstacle avoidance
Task 2 running: OLED display: SHT20 temperature/humidity + AP3216C IR/ALS/PS
Task 3 running: WiFi connect
Task 4 running: Huawei Cloud MQTT realtime upload
```

If the serial port still prints `AP3216C ir=..., als=..., ps=..., led=...`, the board is still running the old `9.0AP3216` firmware, not this project.

## Hi3861 build target

Use this directory's target:

```text
10.0SUM:sum
```

Do not build or burn `9.0AP3216:Ap3216c` for the final car project.

## STM32 side

Burn `TIMER/USER/main.c` from this project after rebuilding the Keil project. The STM32 program waits for 6-byte motor frames from Hi3861 at 115200 baud:

```text
0xFC, left_dir, left_speed, right_dir, right_speed, 0xFD
```

The previous fixed-PWM test code has been removed from the main entry.

The same UART also accepts the light commands used by `9.0AP3216`:

```text
L1: LED on
L0: LED off
YL: left side yellow turn signal
YR: right side yellow turn signal
Y0: turn signal off
```

## WiFi

Edit the WiFi credentials in `sum.c` before burning the Hi3861 firmware:

```c
#define WIFI_SSID "iPhone"
#define WIFI_PSK "nihao1009"
```

Task 3 scans for the configured SSID, connects in station mode, and starts DHCP. If the hotspot is not found or DHCP times out, Task 1 car safety and Task 2 OLED display continue running.

## Huawei Cloud IoTDA

Edit the Huawei Cloud MQTT parameters in `sum.c` before burning the Hi3861 firmware:

```c
#define HUAWEI_IOT_HOST "3c95083845.st1.iotda-device.cn-north-4.myhuaweicloud.com"
#define HUAWEI_IOT_PORT 1883
#define HUAWEI_DEVICE_ID "6a9643457f2e6c302f94fcf9_qstcar"
#define HUAWEI_CLIENT_ID "6a9643457f2e6c302f94fcf9_qstcar_0_0_2026090104"
#define HUAWEI_USERNAME "6a9643457f2e6c302f94fcf9_qstcar"
#define HUAWEI_PASSWORD "b9fc45c0f75d988c8bf279c96d84e6db2c791e47c29b58e6ee42184a467d6134"
```

Task 4 waits until Task 3 has connected WiFi, then connects to Huawei Cloud IoTDA by MQTT and reports car data every 2 seconds.
`sum.c` uses `DelayMs()` because CMSIS `osDelay()` takes OS ticks on this board; with a 10 ms tick, calling `osDelay(5000)` directly would delay about 50 seconds.

The IoTDA product model should contain service `qstcar`. Reported properties:

```text
temp
humi
lumi
mode_led
car_mode
temperature
humidity
ap_ir
ap_als
ap_ps
edge_left
edge_right
distance_cm
led_on
```

A matching product-model reference file is included at `profile/devicetype-capability.json`.

## Wiring used by Hi3861

- GPIO13: left infrared table-edge sensor
- GPIO14: right infrared table-edge sensor
- Infrared edge level is configured by `IR_EDGE_VALUE` in `sum.c`; current default is `HI_GPIO_VALUE1`.
- GPIO7: HC-SR04 Trig
- GPIO8: HC-SR04 Echo
- GPIO2: SG90 signal
- GPIO11/GPIO12: UART2 TX/RX to STM32, 115200 baud
- Bluetooth UART1 is not used; Task 3 is WiFi.
- GPIO9/GPIO10: I2C0 SCL/SDA for OLED, SHT20 and AP3216C
- GPIO6: LED, same as `9.0AP3216`: high is off, ALS <= 50 drives low/on


# Smart Car Realtime Web Dashboard

This directory contains a **Vue 3 + Java** dashboard for smart-car realtime data visualization with **AMQP push** support.

## 📁 Files

| File | Purpose |
|------|----------|
| `car.html` | Vue 3 frontend: realtime charts, status monitor, polling controls |
| `CarCloudServer.java` | Java backend: HTTP server, AMQP consumer, Huawei Cloud proxy |
| `pom.xml` | Maven dependencies (AMQP, SLF4J) |
| `start.bat` | Windows batch launcher |
| `start.ps1` | Windows PowerShell launcher |
| `build.ps1` | Build script (compile only) |

## 🚀 Quick Start

### Option 1: Batch Script (Windows)

```batch
cd C:\Users\18500\Desktop\summer\test\test3-html
start.bat
```

Then open: http://localhost:8080/car.html

### Option 2: PowerShell Script (Windows)

```powershell
cd C:\Users\18500\Desktop\summer\test\test3-html
powershell -ExecutionPolicy Bypass -File start.ps1
```

### Option 3: Manual Maven (any OS)

```bash
cd C:\Users\18500\Desktop\summer\test\test3-html
mvn clean compile
mvn exec:java -Dexec.mainClass="CarCloudServer"
```

### Option 4: Direct Java (after compilation)

```bash
cd C:\Users\18500\Desktop\summer\test\test3-html
javac CarCloudServer.java
java -cp ".:target/classes:target/dependency/*" CarCloudServer
```

## 📡 Access the Dashboard

Open in browser:
```text
http://localhost:8080/car.html
```

The frontend polls `/api/latest` every 2 seconds (configurable).

## 📊 API Endpoints

| Endpoint | Method | Response |
|----------|--------|----------|
| `/car.html` | GET | HTML dashboard |
| `/api/latest` | GET | Latest sensor data + metadata |
| `/api/history` | GET | Historical data array (240 points) |
| `/api/config` | GET | Server configuration info |

## Huawei Cloud Mode

The browser must not store Huawei Cloud credentials. Put credentials only in the Java process environment.

Recommended AK/SK mode for the IoTDA standard instance:

```powershell
$env:HUAWEI_AMQP_ACCESS_KEY="your_amqp_access_key"
$env:HUAWEI_AMQP_ACCESS_CODE="your_amqp_access_code"
```

For AMQP, use the access credential created under the IoTDA AMQP message queue page. Do not use IAM `Access Key Id` / `Secret Access Key` here.

The backend defaults to derived AK/SK signing for IoTDA standard/enterprise instances:

```powershell
$env:HUAWEI_AUTH_TYPE="derived"
$env:HUAWEI_REGION_ID="cn-north-4"
$env:HUAWEI_DERIVED_SERVICE_NAME="iotda"
```

For a basic instance, switch back to normal AK/SK signing:

```powershell
$env:HUAWEI_AUTH_TYPE="normal"
```

`HUAWEI_PROJECT_ID` defaults to this project:

```text
01a05aee135276f59f135b5342b239bb
```

Token mode is still supported if an IAM token is already available:

```powershell
$env:HUAWEI_IAM_TOKEN="your_x_auth_token"
```

Optional variables:

```powershell
$env:HUAWEI_PROJECT_ID="01a05aee135276f59f135b5342b239bb"
$env:HUAWEI_INSTANCE_ID="5357e9ef-bcdf-4934-9c94-2924c34fde2b"
$env:HUAWEI_DEVICE_ID="6a9643457f2e6c302f94fcf9_qstcar"
$env:HUAWEI_SERVICE_ID="qstcar"
$env:HUAWEI_IOTDA_ENDPOINT="https://3c95083845.st1.iotda-app.cn-north-4.myhuaweicloud.com"
$env:HUAWEI_READ_MODE="amqp"
$env:HUAWEI_AMQP_HOST="3c95083845.st1.iotda-app.cn-north-4.myhuaweicloud.com"
$env:HUAWEI_AMQP_PORT="5671"
$env:HUAWEI_AMQP_QUEUE="qst_queue"
$env:CAR_SERVER_PORT="8080"
```

For the current standard IoTDA instance, do not use the MQTT host prefix `3c95083845` as `HUAWEI_INSTANCE_ID`. Use the full UUID-style instance ID: `5357e9ef-bcdf-4934-9c94-2924c34fde2b`.

## 🔄 Read Modes

The backend supports three data-source modes via `HUAWEI_READ_MODE`:

### Mode 1: AMQP (Push) - ⭐ Recommended

```powershell
$env:HUAWEI_READ_MODE="amqp"
```

**Characteristics:**
- Latency: **<50ms** (push-based)
- Updates: Real-time when messages arrive
- Requirement: AMQP broker credentials
- Reliability: Automatic reconnection with backoff
- Status tracking: `amqpStatus` in `/api/config`

**How it works:**
1. Backend daemon thread connects to Huawei AMQP broker (port 5671)
2. Continuously listens for device property messages
3. On message arrival, updates in-memory snapshot
4. Frontend polls `/api/latest` and gets latest data immediately

### Mode 2: Properties (REST Poll) - Fallback

```powershell
$env:HUAWEI_READ_MODE="properties"
```

**Characteristics:**
- Latency: **100-200ms** per request
- Updates: On-demand via REST API
- Requirement: AK/SK or IAM token
- Reliability: Stable, no state management
- Best for: Occasional checks, testing

### Mode 3: Shadow - Stable

```powershell
$env:HUAWEI_READ_MODE="shadow"
```

**Characteristics:**
- Latency: **100-200ms** per request  
- Updates: Device shadow reflection
- Requirement: AK/SK or IAM token
- Reliability: Very stable, broker-managed
- Note: Shadow timestamp may lag behind live updates

## 🔐 Credentials Security

⚠️ **NEVER commit credentials to source control**

```bash
# ❌ DON'T DO THIS
set HUAWEICLOUD_SDK_AK=JpadGfUK
set HUAWEICLOUD_SDK_SK=hDDms2ZYfrMfXvRpqgfUW2tJqnSUya8D
```

✅ **DO THIS instead:**

```powershell
# Option A: Set in terminal before running
$env:HUAWEICLOUD_SDK_AK="your_ak"
$env:HUAWEICLOUD_SDK_SK="your_sk"
start.bat

# Option B: Environment file (local, .gitignored)
# File: .env.local (not committed)
# Then: dot-source it in start.ps1
```

## 📈 Frontend Features

**Realtime Charts** (4 panels):
- Temperature / Humidity (dual-axis)
- AP3216C Light/IR/PS (multi-series)
- Ultrasonic Distance (single)
- Edge Sensor / LED (binary state)

**Status Indicators**:
- Green dot: Connected + live data
- Orange dot: Connected + stale data (>15s)
- Red dot: Disconnected or error

**Polling Controls**:
- Adjustable interval (1-60s)
- Adjustable history size (30-600 points)
- Start/Stop buttons
- Auto-refresh on parameter change

**Data Metadata**:
- Read mode (amqp/properties/shadow)
- Event age (how old the cloud data is)
- Stale flag (warning if >15s)
- Error messages if connection fails

## 🛠️ Troubleshooting

### Issue: "AMQP library not found"
**Solution:** Run with Maven to include dependencies
```powershell
start.ps1  # or start.bat
```

### Issue: "Error: cloud_fetch_failed"
**Check:**
1. Credentials: `echo $env:HUAWEICLOUD_SDK_AK`
2. Network: `ping 3c95083845.st1.iotda-app.cn-north-4.myhuaweicloud.com`
3. Port: AMQP uses 5671, REST uses 443
4. Read mode: May auto-fallback to mock

### Issue: Slow updates
**Check:**
1. Polling interval: Default 2s (reduce to 500ms)
2. Read mode: AMQP fastest, Shadow slower
3. Network latency: Use ping to measure

### Issue: Port 8080 already in use
**Solution:**
```powershell
$env:CAR_SERVER_PORT="8081"
start.ps1
# Then open: http://localhost:8081/car.html
```

## 📚 Architecture

```
Browser (Vue 3)
   ↓ GET /api/latest (poll 2s)
   ↓
CarCloudServer (Java)
   ├─ Daemon: AMQP consumer
   │  ├─ Connect AMQPS broker
   │  ├─ Subscribe queue
   │  ├─ Update lastCloudSnapshot
   │  └─ Auto-reconnect on failure
   │
   └─ HTTP handler
      ├─ Read lastCloudSnapshot
      ├─ Add metadata (read_mode, stale, event_age)
      └─ Return JSON
         ↓
   Browser (update charts)
```

## 📦 Dependencies

```xml
<!-- Maven pom.xml -->
<dependency>
  <groupId>org.apache.qpid</groupId>
  <artifactId>qpid-jms-client</artifactId>
  <version>0.61.0</version>
</dependency>
```

Automatically resolved by Maven when running `start.bat` or `start.ps1`.

## 📝 License

Demo project for smart-car realtime monitoring system.

---

**Recommended startup:**

```batch
REM Windows Command Prompt
cd C:\Users\18500\Desktop\summer\test\test3-html
set HUAWEICLOUD_SDK_AK=your_ak
set HUAWEICLOUD_SDK_SK=your_sk
start.bat
```

```powershell
# Windows PowerShell
cd C:\Users\18500\Desktop\summer\test\test3-html
$env:HUAWEICLOUD_SDK_AK="your_ak"
$env:HUAWEICLOUD_SDK_SK="your_sk"
powershell -ExecutionPolicy Bypass -File start.ps1
```

Then open: **http://localhost:8080/car.html**

Realtime query mode:

```powershell
$env:HUAWEI_READ_MODE="properties"
```

`properties` calls the IoTDA query-device-properties API. It is more direct, but the MQTT device must subscribe to the cloud property-query topic and publish a response. The current Hi3861 firmware only reports properties, so this mode may return a timeout until property-query response support is added.

## Data Source

Cloud mode calls the IoTDA shadow API:

```text
GET /v5/iot/{project_id}/devices/{device_id}/shadow
Header: X-Auth-Token: {token}
```

The dashboard reads service `qstcar` and plots these fields:

```text
temperature, humidity, ap_ir, ap_als, ap_ps, distance_cm, edge_left, edge_right, led_on
```
