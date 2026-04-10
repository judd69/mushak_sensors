# Mushak Sensors

This repository contains the embedded firmware, sensor integration code, and simulation scripts developed for the Mushak rover platform. The codebase covers CO2 air quality monitoring, soil moisture measurement, pH sensing, RC-controlled drive, and wheel terramechanics simulation.

---

## Repository Contents

| File | Description |
|---|---|
| `final_code.cpp` | Main ESP32 firmware integrating all sensors, motor control, RC input, and a web dashboard |
| `Arduino co2 sensor code.cpp` | Standalone Arduino sketch for the Adafruit SCD30 CO2/temperature/humidity sensor |
| `esp32 code mavlink setup for ph sensor` | ESP32 sketch that reads pH and forwards the value over MAVLink serial |
| `wheel_sim.py` | PyChrono wheel simulation on SCM (soft deformable) terrain |
| `wheel_sim_withcom.py` | PyChrono wheel simulation on rigid ground with full telemetry logging |

---

## Hardware

### Main Controller (`final_code.cpp`)

| Component | Detail |
|---|---|
| Microcontroller | ESP32 |
| CO2 / Temperature / Humidity | Sensirion SCD4x (I2C, address `0x62`, pins SDA=21 SCL=22) |
| pH Sensor | Analog, GPIO 34 |
| Soil Moisture Sensor | Analog, GPIO 35 |
| RC Receiver | FlySky IBus, GPIO 16 (UART2 RX) |
| Drive Motors | 4-wheel differential, L/R PWM+DIR pairs (GPIOs 32/33, 25/26, 27/14, 12/13) |
| Water Pump | PWM GPIO 18, DIR GPIO 19 |
| Servo Arm | GPIO 4 |
| Tool Button | GPIO 23 (INPUT_PULLUP) |

### Arduino CO2 Sketch (`Arduino co2 sensor code.cpp`)

| Component | Detail |
|---|---|
| Microcontroller | Arduino (any with I2C) |
| Sensor | Adafruit SCD30 (I2C) |

### MAVLink pH Bridge (`esp32 code mavlink setup for ph sensor`)

| Component | Detail |
|---|---|
| Microcontroller | ESP32 |
| pH Sensor | Analog, GPIO 32 |
| MAVLink output | UART2 (RX=16, TX=17) at 115200 baud |

---

## Dependencies

### ESP32 / Arduino Libraries

- [IBusBM](https://github.com/bmellink/IBusBM) — FlySky IBus RC receiver
- [SensirionI2cScd4x](https://github.com/Sensirion/arduino-i2c-scd4x) — SCD4x CO2 sensor
- [Adafruit SCD30](https://github.com/adafruit/Adafruit_SCD30) — SCD30 CO2 sensor
- [ESP32Servo](https://github.com/madhephaestus/ESP32Servo) — Servo control on ESP32
- [MAVLink](https://github.com/mavlink/c_library_v2) — MAVLink protocol (C headers)
- Built-in Arduino/ESP32 libraries: `Wire`, `WiFi`, `WebServer`

### Python Simulation

- [PyChrono](https://projectchrono.org/) — Physics simulation engine with SCM terrain support
- Python standard library: `csv`, `os`, `math`

---

## Configuration

Before flashing `final_code.cpp`, update the Wi-Fi credentials:

```cpp
static const char* WIFI_SSID     = "YOUR_WIFI_SSID";
static const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
```

If the station connection fails, the ESP32 falls back to an access point:

- **SSID:** `Mushak-ESP32`
- **Password:** `mushak123`

---

## Usage

### Main ESP32 Firmware

Flash `final_code.cpp` with the Arduino IDE or PlatformIO targeting an ESP32 board. After boot, the serial monitor at 115200 baud reports the assigned IP address. Navigate to that address in a browser to open the dashboard.

**RC Channel Mapping**

| Channel | Function |
|---|---|
| CH1 | Steering |
| CH2 | Throttle |
| CH3 | Brake (active when < 1750) |
| CH4 | Speed limiter disable (high = full speed) |
| CH5 > 1800 | Trigger CO2 measurement (30 s) |
| CH5 1300–1700 | Queue water sampling task |
| CH6 > 1500 | Queue soil sampling task |

**Tool Button (GPIO 23)**

| Press | Action |
|---|---|
| Short press (< 1.2 s) | Queue water sampling sequence |
| Long press (>= 1.2 s) | Queue soil sampling task |

### Web Dashboard

The ESP32 exposes a browser dashboard and a REST API.

| Endpoint | Method | Description |
|---|---|---|
| `/` | GET | HTML dashboard with live status and task buttons |
| `/api/status` | GET | JSON object with all sensor readings and system state |
| `/api/water/start` | GET | Queue the water sampling sequence |
| `/api/soil/read` | GET | Queue the soil sampling task |

Example `/api/status` response:

```json
{
  "uptime_ms": 12345,
  "tool_busy": false,
  "co2_state": "idle",
  "co2_ppm": 412,
  "temperature_c": 24.50,
  "humidity_pct": 55.10,
  "ph": 6.85,
  "ph_raw": 1820,
  "soil_raw": 2200,
  "soil_moisture_pct": 50.0,
  "water_runs": 2,
  "soil_runs": 1,
  "wifi_mode": "station"
}
```

### CO2 Air Quality Thresholds (Arduino Sketch)

| CO2 (ppm) | Status |
|---|---|
| 0 – 350 | Excellent (healthy outside air) |
| 351 – 600 | Good (healthy indoor climate) |
| 601 – 800 | Fair (acceptable) |
| 801 – 1000 | Poor (ventilation required) |
| 1001 – 1200 | Bad (ventilation necessary) |
| 1201 – 2500 | Very Bad (negative health effects) |
| > 2500 | Danger (hazardous) |

---

## Wheel Simulation

Both simulation scripts use PyChrono to model a single driven wheel and log terramechanics data to CSV.

**Common parameters**

| Parameter | Value |
|---|---|
| Wheel radius | 0.125 m |
| Target speed | 8 km/h |
| Total test mass | 260 kg |

**`wheel_sim.py`** — SCM deformable terrain, logs: time, sinkage, tractive effort, actual velocity. Output file: `sim_output_results.csv`.

**`wheel_sim_withcom.py`** — Rigid ground baseline, logs: time, target speed, actual speed, sinkage, slip ratio, vertical load, drawbar pull, tractive efficiency, compliance status. Output file: `pakka_pakka_sim_results.csv`.

Run either script from the directory containing the corresponding `.obj` wheel mesh file:

```bash
python wheel_sim.py
python wheel_sim_withcom.py
```

---

## License

This project does not currently specify a license. Contact the repository owner for usage terms.
