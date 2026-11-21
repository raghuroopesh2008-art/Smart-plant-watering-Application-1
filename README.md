# 🌱 Sprout — Smart Plant Watering System

An ESP32-based plant watering controller that measures soil moisture, waters automatically when the soil runs dry, and shows everything on a live web dashboard.

## Features

- **Automatic watering** when soil moisture drops below a set threshold
- **Manual override** to run the pump from the dashboard
- **Cooldown timer** so the plant is never flooded
- **Live dashboard** with a moisture gauge, one-hour trend line, and an event journal
- **Demo mode** so the dashboard works before the hardware is connected

## Hardware

| Part | Qty | Notes |
|---|---|---|
| ESP32 Dev Board | 1 | Wi-Fi controller |
| Capacitive soil moisture sensor v1.2 | 1 | Better than resistive types; does not corrode |
| DHT11 temperature & humidity sensor | 1 | Optional, for air readings |
| 5V relay module (1 channel) | 1 | Switches the pump |
| 5V submersible water pump | 1 | Small DC pump with silicone tubing |
| 5V power supply (2A) | 1 | Powers the pump and board |
| Jumper wires, breadboard, tubing | — | — |

## Wiring

| From | To | Purpose |
|---|---|---|
| Soil sensor `AOUT` | ESP32 `GPIO34` | Moisture reading |
| Soil sensor `VCC` / `GND` | ESP32 `3V3` / `GND` | Power |
| DHT11 `DATA` | ESP32 `GPIO4` | Air temperature and humidity |
| DHT11 `VCC` / `GND` | ESP32 `3V3` / `GND` | Power |
| Relay `IN` | ESP32 `GPIO26` | Pump control |
| Relay `VCC` / `GND` | ESP32 `5V` / `GND` | Power |
| Pump | Relay `COM` and `NO` | Switched by the relay |

> ⚠️ The pump runs on external 5V power. Connect the power supply's negative terminal to the ESP32's `GND` so the signals share a common ground.

## Calibrate the soil sensor

1. Hold the sensor in open air and note the raw value in the serial monitor. Set `SOIL_DRY_RAW` to it.
2. Put the sensor's probe in a cup of water and note the raw value. Set `SOIL_WET_RAW` to it.
3. Keep the sensor's electronics above the waterline. Only the probe should be submerged.

## Software setup

1. Install the **Arduino IDE** and add the ESP32 board package from *Boards Manager*.
2. Install the **DHT sensor library** by Adafruit from *Library Manager*.
3. Open `firmware/smart_plant_watering.ino`, then set your `WIFI_SSID` and `WIFI_PASSWORD`.
4. Select the board **ESP32 Dev Module** and upload.
5. Open the serial monitor at 115200 baud and copy the IP address that appears.
6. Open `dashboard/index.html` in a browser after setting `DEVICE` to that IP address (for example `http://192.168.1.50`).

## API reference

| Method | Endpoint | Description |
|---|---|---|
| `GET` | `/api/status` | Returns soil %, temperature, humidity, pump state, threshold and last event |
| `POST` | `/api/pump?state=on` | Starts manual watering |
| `POST` | `/api/pump?state=off` | Stops manual watering |
| `POST` | `/api/threshold?value=40` | Sets the auto-water threshold (0–100) |

Example response:

```json
{
  "soil": 48,
  "temperature": 24.5,
  "humidity": 58,
  "pump": false,
  "threshold": 35,
  "manual": false,
  "lastEvent": "Watering finished",
  "uptimeMs": 183204
}
```

## Project layout

```
smart-plant-watering/
├── firmware/
│   └── smart_plant_watering.ino   # ESP32 sketch
├── dashboard/
│   └── index.html                 # Single-file web dashboard
├── .gitignore
└── README.md
```

## Ideas for next steps

- Add a water-level float switch so the pump stops when the reservoir is empty
- Log readings to Google Sheets or a local database
- Send a push notification when the plant is thirsty
- Add a solar panel and battery for a fully standalone garden

## License

MIT. Use it, change it, and share it.
