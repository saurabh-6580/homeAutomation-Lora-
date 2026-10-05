# LoRa Smart Home Automation

## Architecture

React -> Flask -> ESP32 -> relays / DHT11 / LDR / servo

Arduino Uno <-> LoRa <-> ESP32 (buttons to ESP32, DHT11 telemetry back to Uno)

The LoRa path executes directly on the ESP32, so the physical remote can still work when Wi-Fi is unavailable.

## Folder structure

```
LoRa-Smart-Home-Automation/
├── frontend/
│   ├── src/
│   │   ├── assets/
│   │   │   └── smart-home-house.png
│   │   ├── components/
│   │   │   ├── Navbar.jsx
│   │   │   ├── SensorBadge.jsx
│   │   │   └── BenefitCard.jsx
│   │   ├── pages/
│   │   │   ├── LandingPage.jsx
│   │   │   └── DashboardPage.jsx
│   │   ├── App.jsx
│   │   ├── main.jsx
│   │   └── styles.css
│   ├── index.html
│   ├── package.json
│   └── vite.config.js
├── backend/
│   ├── app.py
│   └── requirements.txt
└── hardware/
    ├── esp32/
    │   └── esp32_controller.ino
    └── arduino_uno/
        └── lora_remote.ino
```

## Run backend

```bash
cd backend
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
python3 app.py
```

## Run frontend

In another terminal:

```bash
cd frontend
npm install
npm run dev
```

Then open:

http://localhost:5173

## Demo mode

By default Flask does not require an ESP32. The dashboard still loads and controls its stored demo state.

## Hardware mode

Find your ESP32 IP from Serial Monitor, then:

```bash
export ESP32_BASE_URL=http://192.168.1.50
export HARDWARE_MODE=true
python3 app.py
```

Replace the IP with the ESP32's actual IP.

## Find Ubuntu laptop IP

```bash
hostname -I
```

Put that address in the ESP32 sketch:

```cpp
const char* FLASK_BASE_URL = "http://192.168.1.10:5000";
```

## Arduino IDE libraries

ESP32:
- LoRa by Sandeep Mistry
- DHT sensor library by Adafruit
- Adafruit Unified Sensor
- ESP32Servo

Arduino Uno:
- LoRa by Sandeep Mistry

## Important hardware note

Do not connect a typical 3.3V LoRa radio's SPI input pins directly to the Arduino Uno's 5V outputs. Use a stable 3.3V supply and suitable logic-level conversion.

For relay/mains testing, begin with LEDs or low-voltage loads before using household AC.
