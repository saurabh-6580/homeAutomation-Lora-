#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <LoRa.h>
#include <DHT.h>
#include <ESP32Servo.h>

// =====================================================
// WIFI / FLASK BACKEND
// =====================================================
// Keep the Wi-Fi values that already work on your ESP32.
const char* WIFI_SSID = "POCO m6";
const char* WIFI_PASSWORD = "12345678";

// IMPORTANT:
// Use the IP address of the COMPUTER running Flask.
// Keep the same FLASK_BASE_URL that already gives:
// POST Flask status -> 200
// Example: http://192.168.1.10:5000
const char* FLASK_BASE_URL = "http://172.24.254.41:5000";

// =====================================================
// OUTPUT PINS - UNCHANGED
// =====================================================
#define LIGHT_PIN 25
#define FAN_PIN 26
#define OUTDOOR_PIN 27

// =====================================================
// DHT11 / LDR / SERVO - UNCHANGED
// =====================================================
#define DHT_PIN 32
#define DHT_TYPE DHT11
#define LDR_PIN 34
#define SERVO_PIN 33

// =====================================================
// RA-02 LORA - UNCHANGED
// =====================================================
#define LORA_SCK 18
#define LORA_MISO 19
#define LORA_MOSI 23
#define LORA_SS 5
#define LORA_RST 14
#define LORA_DIO0 2
#define LORA_FREQUENCY 433E6

DHT dht(DHT_PIN, DHT_TYPE);
Servo doorServo;

// =====================================================
// DEVICE STATES
// =====================================================
bool roomLight = false;
bool fanOn = false;
bool outdoorLight = false;
bool doorLocked = true;
bool loraConnected = false;

String outdoorMode = "auto";
String environmentState = "Unknown";

// =====================================================
// SENSOR VALUES
// =====================================================
float temperature = NAN;
float humidity = NAN;
int ldrValue = 0;

// =====================================================
// LORA VALUES
// =====================================================
int lastLoraRssi = 0;
float lastLoraSnr = 0;

// =====================================================
// LDR RULE
// =====================================================
// LDR > 3000  -> DARK   -> OUTDOOR LIGHT ON
// LDR <= 3000 -> BRIGHT -> OUTDOOR LIGHT OFF
const int DARK_THRESHOLD = 3000;

// =====================================================
// TIMERS
// =====================================================
unsigned long lastSensorRead = 0;
unsigned long lastFlaskPost = 0;
unsigned long lastDhtLoRaSend = 0;
unsigned long lastWiFiRetry = 0;
unsigned long lastCommandPoll = 0;

const unsigned long SENSOR_INTERVAL = 2000;
const unsigned long FLASK_INTERVAL = 3000;
const unsigned long DHT_LORA_INTERVAL = 5000;
const unsigned long WIFI_RETRY_INTERVAL = 10000;
const unsigned long COMMAND_POLL_INTERVAL = 150;

bool stateDirty = true;

// =====================================================
// HELPERS
// =====================================================
String boolJson(bool value) {
  return value ? "true" : "false";
}

void setRoomLight(bool state) {
  if (roomLight == state) return;

  roomLight = state;
  digitalWrite(LIGHT_PIN, state ? HIGH : LOW);

  Serial.print("Room Light: ");
  Serial.println(state ? "ON" : "OFF");

  stateDirty = true;
}

void setFan(bool state) {
  if (fanOn == state) return;

  fanOn = state;
  digitalWrite(FAN_PIN, state ? HIGH : LOW);

  Serial.print("Fan: ");
  Serial.println(state ? "ON" : "OFF");

  stateDirty = true;
}

void setOutdoorLight(bool state) {
  if (outdoorLight == state) return;

  outdoorLight = state;
  digitalWrite(OUTDOOR_PIN, state ? HIGH : LOW);

  Serial.print("Outdoor Light: ");
  Serial.println(state ? "ON" : "OFF");

  stateDirty = true;
}

void lockDoor() {
  doorServo.write(10);

  if (!doorLocked) {
    Serial.println("Door: LOCKED");
  }

  doorLocked = true;
  stateDirty = true;
}

void unlockDoor() {
  doorServo.write(90);

  if (doorLocked) {
    Serial.println("Door: UNLOCKED");
  }

  doorLocked = false;
  stateDirty = true;
}

// =====================================================
// LDR AUTOMATION
// =====================================================
void updateOutdoorAutomation() {
  // LDR only controls GPIO27 in AUTO mode.
  if (outdoorMode != "auto") return;

  if (ldrValue > DARK_THRESHOLD) {
    environmentState = "Dark";
    setOutdoorLight(true);
  } else {
    environmentState = "Bright";
    setOutdoorLight(false);
  }
}

// =====================================================
// SENSOR READING
// =====================================================
void readSensors() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (!isnan(t) && !isnan(h)) {
    temperature = t;
    humidity = h;
  } else {
    Serial.println("WARNING: DHT11 read failed");
  }

  ldrValue = analogRead(LDR_PIN);

  if (ldrValue > DARK_THRESHOLD) {
    environmentState = "Dark";
  } else {
    environmentState = "Bright";
  }

  updateOutdoorAutomation();

  Serial.print("DHT11 -> T: ");

  if (isnan(temperature)) {
    Serial.print("NA");
  } else {
    Serial.print(temperature, 1);
  }

  Serial.print(" C, H: ");

  if (isnan(humidity)) {
    Serial.print("NA");
  } else {
    Serial.print(humidity, 1);
  }

  Serial.print(" %, LDR: ");
  Serial.print(ldrValue);

  Serial.print(", Environment: ");
  Serial.print(environmentState);

  Serial.print(", Outdoor: ");
  Serial.println(outdoorLight ? "ON" : "OFF");
}

// =====================================================
// STATE JSON
// =====================================================
String makeStateJson() {
  String json = "{";

  json += "\"temp\":";
  json += isnan(temperature) ? "null" : String(temperature, 1);

  json += ",\"humidity\":";
  json += isnan(humidity) ? "null" : String(humidity, 1);

  json += ",\"ldr\":" + String(ldrValue);
  json += ",\"environment\":\"" + environmentState + "\"";
  json += ",\"light\":" + boolJson(roomLight);
  json += ",\"fan\":" + boolJson(fanOn);
  json += ",\"outdoorLight\":" + boolJson(outdoorLight);
  json += ",\"outdoorMode\":\"" + outdoorMode + "\"";
  json += ",\"doorLocked\":" + boolJson(doorLocked);
  json += ",\"wifiConnected\":" + boolJson(WiFi.status() == WL_CONNECTED);
  json += ",\"loraConnected\":" + boolJson(loraConnected);
  json += ",\"loraRssi\":";

  if (loraConnected) {
    json += String(lastLoraRssi);
  } else {
    json += "null";
  }

  json += "}";
  return json;
}

// =====================================================
// SEND ESP32 STATE TO FLASK
// =====================================================
void sendStateToFlask() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.setConnectTimeout(800);
  http.setTimeout(1200);

  String url = String(FLASK_BASE_URL) + "/api/iot/status";

  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  int code = http.POST(makeStateJson());

  Serial.print("POST Flask status -> ");
  Serial.println(code);

  http.end();

  if (code > 0) {
    stateDirty = false;
  }
}

// =====================================================
// SEND LORA EVENT TO FLASK
// =====================================================
void sendLoraEventToFlask(const String &command) {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.setConnectTimeout(800);
  http.setTimeout(1200);

  String url = String(FLASK_BASE_URL) + "/api/lora/event";

  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  String body =
    "{\"command\":\"" + command +
    "\",\"rssi\":" + String(lastLoraRssi) +
    ",\"state\":" + makeStateJson() + "}";

  int code = http.POST(body);

  Serial.print("POST LoRa event -> ");
  Serial.println(code);

  http.end();
}

// =====================================================
// SEND DHT11 TELEMETRY TO ARDUINO UNO THROUGH LORA
// =====================================================
void sendDhtToArduino() {
  if (isnan(temperature) || isnan(humidity)) return;

  String packet =
    "DHT|" + String(temperature, 1) + "|" + String(humidity, 1);

  Serial.print("Sending DHT11 to UNO: ");
  Serial.println(packet);

  LoRa.idle();
  delay(10);

  LoRa.beginPacket();
  LoRa.print(packet);

  int result = LoRa.endPacket();

  Serial.print("DHT packet result: ");
  Serial.println(result);

  delay(20);

  // Important: return radio to receive mode for Uno button commands.
  LoRa.receive();
}

// =====================================================
// EXECUTE COMMANDS RECEIVED FROM ARDUINO UNO VIA LORA
// =====================================================
String executeCommand(String command) {
  command.trim();

  if (command == "LIGHT|ON") {
    setRoomLight(true);
    return "LIGHT ON";
  }

  if (command == "LIGHT|OFF") {
    setRoomLight(false);
    return "LIGHT OFF";
  }

  if (command == "LIGHT|TOGGLE") {
    setRoomLight(!roomLight);
    return roomLight ? "LIGHT ON" : "LIGHT OFF";
  }

  if (command == "FAN|ON") {
    setFan(true);
    return "FAN ON";
  }

  if (command == "FAN|OFF") {
    setFan(false);
    return "FAN OFF";
  }

  if (command == "FAN|TOGGLE") {
    setFan(!fanOn);
    return fanOn ? "FAN ON" : "FAN OFF";
  }

  if (command == "OUTDOOR|AUTO") {
    outdoorMode = "auto";
    updateOutdoorAutomation();
    stateDirty = true;
    return "OUTDOOR AUTO";
  }

  if (command == "OUTDOOR|MANUAL") {
    outdoorMode = "manual";
    stateDirty = true;
    return "OUTDOOR MANUAL";
  }

  if (command == "OUTDOOR|ON") {
    outdoorMode = "manual";
    setOutdoorLight(true);
    return "OUTDOOR ON";
  }

  if (command == "OUTDOOR|OFF") {
    outdoorMode = "manual";
    setOutdoorLight(false);
    return "OUTDOOR OFF";
  }

  if (command == "OUTDOOR|TOGGLE") {
    outdoorMode = "manual";
    setOutdoorLight(!outdoorLight);
    return outdoorLight ? "OUTDOOR ON" : "OUTDOOR OFF";
  }

  if (command == "DOOR|LOCK") {
    lockDoor();
    return "DOOR LOCKED";
  }

  if (command == "DOOR|UNLOCK") {
    unlockDoor();
    return "DOOR UNLOCKED";
  }

  if (command == "DOOR|TOGGLE") {
    if (doorLocked) {
      unlockDoor();
    } else {
      lockDoor();
    }

    return doorLocked ? "DOOR LOCKED" : "DOOR UNLOCKED";
  }

  return "UNKNOWN COMMAND";
}

// =====================================================
// CHECK ARDUINO UNO LORA COMMANDS
// =====================================================
void checkLoRa() {
  int packetSize = LoRa.parsePacket();

  if (!packetSize) return;

  String command = "";

  while (LoRa.available()) {
    command += (char)LoRa.read();
  }

  command.trim();

  lastLoraRssi = LoRa.packetRssi();
  lastLoraSnr = LoRa.packetSnr();
  loraConnected = true;

  Serial.println();
  Serial.println("================================");
  Serial.println("LORA COMMAND FROM UNO");
  Serial.print("Command: ");
  Serial.println(command);
  Serial.print("RSSI: ");
  Serial.print(lastLoraRssi);
  Serial.println(" dBm");
  Serial.print("SNR: ");
  Serial.println(lastLoraSnr);

  String result = executeCommand(command);

  Serial.print("Result: ");
  Serial.println(result);
  Serial.println("================================");

  // Stay ready for the next Uno command.
  LoRa.receive();

  sendLoraEventToFlask(command);
  stateDirty = true;
}

// =====================================================
// EXECUTE WEBSITE COMMAND POLLED FROM FLASK
// Flask sends one of:
// CONTROL|light|1
// CONTROL|fan|0
// CONTROL|outdoorLight|1
// CONTROL|door|1
// MODE|auto
// MODE|manual
// =====================================================
void executeFlaskCommand(String command) {
  command.trim();

  Serial.println();
  Serial.println("================================");
  Serial.print("FLASK COMMAND: ");
  Serial.println(command);

  if (command.startsWith("CONTROL|")) {
    int firstSeparator = command.indexOf('|');
    int secondSeparator = command.indexOf('|', firstSeparator + 1);

    if (firstSeparator < 0 || secondSeparator < 0) {
      Serial.println("Invalid CONTROL command");
      Serial.println("================================");
      return;
    }

    String device = command.substring(firstSeparator + 1, secondSeparator);
    String stateText = command.substring(secondSeparator + 1);

    bool state = (stateText == "1");

    if (device == "light") {
      setRoomLight(state);
    }
    else if (device == "fan") {
      setFan(state);
    }
    else if (device == "outdoorLight") {
      // Website manual control means manual mode.
      outdoorMode = "manual";
      setOutdoorLight(state);
    }
    else if (device == "door") {
      // Keep existing website meaning:
      // state 1 = unlock
      // state 0 = lock
      if (state) {
        unlockDoor();
      } else {
        lockDoor();
      }
    }
    else {
      Serial.println("Unknown website device");
      Serial.println("================================");
      return;
    }
  }
  else if (command.startsWith("MODE|")) {
    String mode = command.substring(5);
    mode.trim();

    if (mode == "auto") {
      outdoorMode = "auto";
      updateOutdoorAutomation();
      stateDirty = true;

      Serial.println("Outdoor Mode: AUTO");
    }
    else if (mode == "manual") {
      outdoorMode = "manual";
      stateDirty = true;

      Serial.println("Outdoor Mode: MANUAL");
    }
    else {
      Serial.println("Invalid outdoor mode command");
      Serial.println("================================");
      return;
    }
  }
  else {
    Serial.println("Unknown Flask command");
    Serial.println("================================");
    return;
  }

  Serial.println("================================");

  // Send authoritative state back immediately after executing a website command.
  sendStateToFlask();
}

// =====================================================
// ESP32 POLLS FLASK FOR WEBSITE COMMANDS
// =====================================================
void pollFlaskCommand() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;

  // Short timeouts keep Wi-Fi polling from blocking LoRa for long periods.
  http.setConnectTimeout(200);
  http.setTimeout(300);

  String url = String(FLASK_BASE_URL) + "/api/iot/command";

  http.begin(url);

  int code = http.GET();

  if (code == 200) {
    String command = http.getString();
    command.trim();

    http.end();

    if (command.length() > 0 && command != "NONE") {
      executeFlaskCommand(command);
    }

    return;
  }

  // Avoid printing errors every second if Flask is temporarily unavailable.
  if (code < 0) {
    Serial.print("Flask command poll failed: ");
    Serial.println(code);
  }

  http.end();
}

// =====================================================
// WIFI
// =====================================================
void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.print("Connecting to Wi-Fi");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();

  while (
    WiFi.status() != WL_CONNECTED &&
    millis() - start < 15000
  ) {
    // Keep checking LoRa even while Wi-Fi is connecting.
    checkLoRa();

    delay(250);
    Serial.print('.');
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("ESP32 IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Wi-Fi not connected. LoRa still works.");
  }
}

// =====================================================
// SETUP
// =====================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("================================");
  Serial.println("ESP32 SMART HOME");
  Serial.println("Flask control: FAST ESP32 POLLING (150 ms)");
  Serial.println("LDR > 3000 = DARK = OUTDOOR ON");
  Serial.println("LDR <= 3000 = BRIGHT = OUTDOOR OFF");
  Serial.println("================================");

  pinMode(LIGHT_PIN, OUTPUT);
  pinMode(FAN_PIN, OUTPUT);
  pinMode(OUTDOOR_PIN, OUTPUT);

  digitalWrite(LIGHT_PIN, LOW);
  digitalWrite(FAN_PIN, LOW);
  digitalWrite(OUTDOOR_PIN, LOW);

  dht.begin();

  doorServo.setPeriodHertz(50);
  doorServo.attach(SERVO_PIN, 500, 2400);
  lockDoor();

  // Initialize LoRa before Wi-Fi so Arduino remote control becomes available
  // as early as possible.
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(LORA_FREQUENCY)) {
    Serial.println("ERROR: LoRa initialization failed!");

    while (true) {
      delay(1000);
    }
  }

  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);
  LoRa.setSyncWord(0x12);
  LoRa.enableCrc();
  LoRa.receive();

  Serial.println("LoRa ready at 433 MHz");

  connectWiFi();

  delay(2000);

  readSensors();
  sendStateToFlask();
}

// =====================================================
// LOOP
// =====================================================
void loop() {
  // Keep LoRa responsive for Arduino Uno button commands.
  checkLoRa();

  unsigned long now = millis();

  // Website/Flask commands: poll very frequently for fast response.
  // With 150 ms polling, a website click should normally reach the ESP32
  // in well under 1 second on a healthy Wi-Fi connection.
  if (now - lastCommandPoll >= COMMAND_POLL_INTERVAL) {
    lastCommandPoll = now;
    pollFlaskCommand();

    // Check LoRa again immediately after the HTTP request so Arduino
    // remote commands are not unnecessarily delayed by Wi-Fi polling.
    checkLoRa();
  }

  // Read DHT11 + LDR every 2 seconds.
  if (now - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = now;
    readSensors();
  }

  // Send DHT telemetry to Arduino Uno every 5 seconds.
  if (now - lastDhtLoRaSend >= DHT_LORA_INTERVAL) {
    lastDhtLoRaSend = now;
    sendDhtToArduino();
  }

  // Wi-Fi reconnect if required. LoRa does not depend on Wi-Fi.
  if (
    WiFi.status() != WL_CONNECTED &&
    now - lastWiFiRetry >= WIFI_RETRY_INTERVAL
  ) {
    lastWiFiRetry = now;

    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }

  // Send current state to Flask periodically or immediately after a change.
  if (
    now - lastFlaskPost >= FLASK_INTERVAL ||
    stateDirty
  ) {
    lastFlaskPost = now;
    sendStateToFlask();
  }
}
