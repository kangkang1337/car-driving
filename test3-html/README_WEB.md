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
