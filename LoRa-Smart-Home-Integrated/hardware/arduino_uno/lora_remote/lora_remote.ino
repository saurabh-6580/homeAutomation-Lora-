#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <LoRa.h>
#include <DHT.h>
#include <ESP32Servo.h>

// =====================================================
// WIFI / BACKEND
// =====================================================

const char* WIFI_SSID =
  "POCO m6";

const char* WIFI_PASSWORD =
  "12345678";

// IMPORTANT:
// This must be the IP of the COMPUTER running Flask,
// NOT 127.0.0.1.
//
// Example:
// http://10.213.201.XX:5000

const char* FLASK_BASE_URL =
  "http://192.168.157.1:5000";

// =====================================================
// OUTPUT PINS
// =====================================================

#define LIGHT_PIN       25
#define FAN_PIN         26
#define OUTDOOR_PIN     27

// =====================================================
// DHT11 / LDR / SERVO
// =====================================================

#define DHT_PIN         32
#define DHT_TYPE        DHT11

#define LDR_PIN         34

#define SERVO_PIN       33

// =====================================================
// RA-02 LORA
// =====================================================

#define LORA_SCK        18
#define LORA_MISO       19
#define LORA_MOSI       23
#define LORA_SS         5
#define LORA_RST        14
#define LORA_DIO0       2

#define LORA_FREQUENCY  433E6

// =====================================================
// OBJECTS
// =====================================================

WebServer server(80);

DHT dht(
  DHT_PIN,
  DHT_TYPE
);

Servo doorServo;

// =====================================================
// DEVICE STATES
// =====================================================

bool roomLight = false;
bool fanOn = false;
bool outdoorLight = false;
bool doorLocked = true;

bool loraConnected = false;

String outdoorMode =
  "auto";

String environmentState =
  "Unknown";

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
// NEW LDR THRESHOLD
// =====================================================

// LDR > 3000 = DARK
// LDR <= 3000 = BRIGHT

const int DARK_THRESHOLD = 3000;

// =====================================================
// TIMERS
// =====================================================

unsigned long lastSensorRead = 0;
unsigned long lastFlaskPost = 0;
unsigned long lastDhtLoRaSend = 0;
unsigned long lastWiFiRetry = 0;

const unsigned long SENSOR_INTERVAL =
  2000;

const unsigned long FLASK_INTERVAL =
  3000;

const unsigned long DHT_LORA_INTERVAL =
  5000;

const unsigned long WIFI_RETRY_INTERVAL =
  10000;

// =====================================================
// STATE CHANGE FLAG
// =====================================================

bool stateDirty = true;

// =====================================================
// BOOL TO JSON
// =====================================================

String boolJson(bool value)
{
  return value
    ? "true"
    : "false";
}

// =====================================================
// ROOM LIGHT
// =====================================================

void setRoomLight(bool state)
{
  roomLight = state;

  digitalWrite(
    LIGHT_PIN,
    state ? HIGH : LOW
  );

  Serial.print(
    "Room Light: "
  );

  Serial.println(
    state ? "ON" : "OFF"
  );

  stateDirty = true;
}

// =====================================================
// FAN
// =====================================================

void setFan(bool state)
{
  fanOn = state;

  digitalWrite(
    FAN_PIN,
    state ? HIGH : LOW
  );

  Serial.print(
    "Fan: "
  );

  Serial.println(
    state ? "ON" : "OFF"
  );

  stateDirty = true;
}

// =====================================================
// OUTDOOR LIGHT
// =====================================================

void setOutdoorLight(bool state)
{
  // Avoid unnecessary repeated writes
  if (
    outdoorLight == state
  )
  {
    return;
  }

  outdoorLight = state;

  digitalWrite(
    OUTDOOR_PIN,
    state ? HIGH : LOW
  );

  Serial.print(
    "Outdoor Light: "
  );

  Serial.println(
    state ? "ON" : "OFF"
  );

  stateDirty = true;
}

// =====================================================
// DOOR LOCK
// =====================================================

void lockDoor()
{
  doorServo.write(10);

  doorLocked = true;

  Serial.println(
    "Door: LOCKED"
  );

  stateDirty = true;
}

// =====================================================
// DOOR UNLOCK
// =====================================================

void unlockDoor()
{
  doorServo.write(90);

  doorLocked = false;

  Serial.println(
    "Door: UNLOCKED"
  );

  stateDirty = true;
}

// =====================================================
// OUTDOOR AUTOMATIC CONTROL
// =====================================================

void updateOutdoorAutomation()
{
  // LDR must not control outdoor light
  // while manual mode is selected

  if (
    outdoorMode != "auto"
  )
  {
    return;
  }

  // ===================================================
  // LDR ABOVE 3000 = DARK
  // OUTDOOR LIGHT ON
  // ===================================================

  if (
    ldrValue > DARK_THRESHOLD
  )
  {
    environmentState =
      "Dark";

    setOutdoorLight(
      true
    );
  }

  // ===================================================
  // LDR 3000 OR BELOW = BRIGHT
  // OUTDOOR LIGHT OFF
  // ===================================================

  else
  {
    environmentState =
      "Bright";

    setOutdoorLight(
      false
    );
  }
}

// =====================================================
// READ DHT11 + LDR
// =====================================================

void readSensors()
{
  // ===================================================
  // DHT11
  // ===================================================

  float t =
    dht.readTemperature();

  float h =
    dht.readHumidity();

  if (
    !isnan(t) &&
    !isnan(h)
  )
  {
    temperature = t;

    humidity = h;
  }
  else
  {
    Serial.println(
      "WARNING: DHT11 read failed"
    );
  }

  // ===================================================
  // READ LDR GPIO34
  // ===================================================

  ldrValue =
    analogRead(
      LDR_PIN
    );

  // ===================================================
  // NEW ENVIRONMENT LOGIC
  // ===================================================

  if (
    ldrValue > DARK_THRESHOLD
  )
  {
    environmentState =
      "Dark";
  }
  else
  {
    environmentState =
      "Bright";
  }

  // ===================================================
  // AUTOMATIC OUTDOOR LIGHT
  // ===================================================

  updateOutdoorAutomation();

  stateDirty = true;

  // ===================================================
  // SERIAL MONITOR
  // ===================================================

  Serial.print(
    "DHT11 -> T: "
  );

  if (
    isnan(temperature)
  )
  {
    Serial.print("NA");
  }
  else
  {
    Serial.print(
      temperature,
      1
    );
  }

  Serial.print(
    " C, H: "
  );

  if (
    isnan(humidity)
  )
  {
    Serial.print("NA");
  }
  else
  {
    Serial.print(
      humidity,
      1
    );
  }

  Serial.print(
    " %, LDR: "
  );

  Serial.print(
    ldrValue
  );

  Serial.print(
    ", Environment: "
  );

  Serial.print(
    environmentState
  );

  Serial.print(
    ", Outdoor: "
  );

  Serial.println(
    outdoorLight
      ? "ON"
      : "OFF"
  );
}

// =====================================================
// CREATE STATE JSON
// =====================================================

String makeStateJson()
{
  String json = "{";

  // Temperature
  json += "\"temp\":";

  json += isnan(temperature)
    ? "null"
    : String(temperature, 1);

  // Humidity
  json += ",\"humidity\":";

  json += isnan(humidity)
    ? "null"
    : String(humidity, 1);

  // LDR
  json +=
    ",\"ldr\":" +
    String(ldrValue);

  // Environment
  json +=
    ",\"environment\":\"" +
    environmentState +
    "\"";

  // Room light
  json +=
    ",\"light\":" +
    boolJson(roomLight);

  // Fan
  json +=
    ",\"fan\":" +
    boolJson(fanOn);

  // Outdoor
  json +=
    ",\"outdoorLight\":" +
    boolJson(outdoorLight);

  // Outdoor mode
  json +=
    ",\"outdoorMode\":\"" +
    outdoorMode +
    "\"";

  // Door
  json +=
    ",\"doorLocked\":" +
    boolJson(doorLocked);

  // Wi-Fi
  json +=
    ",\"wifiConnected\":" +
    boolJson(
      WiFi.status() ==
      WL_CONNECTED
    );

  // LoRa
  json +=
    ",\"loraConnected\":" +
    boolJson(
      loraConnected
    );

  // LoRa RSSI
  json +=
    ",\"loraRssi\":";

  if (
    loraConnected
  )
  {
    json +=
      String(
        lastLoraRssi
      );
  }
  else
  {
    json += "null";
  }

  json += "}";

  return json;
}

// =====================================================
// SEND STATUS TO FLASK
// =====================================================

void sendStateToFlask()
{
  if (
    WiFi.status() !=
    WL_CONNECTED
  )
  {
    return;
  }

  HTTPClient http;

  http.setConnectTimeout(
    1200
  );

  http.setTimeout(
    1500
  );

  String url =
    String(
      FLASK_BASE_URL
    )
    +
    "/api/iot/status";

  http.begin(url);

  http.addHeader(
    "Content-Type",
    "application/json"
  );

  int code =
    http.POST(
      makeStateJson()
    );

  Serial.print(
    "POST Flask status -> "
  );

  Serial.println(
    code
  );

  http.end();

  if (
    code > 0
  )
  {
    stateDirty = false;
  }
}

// =====================================================
// SEND LORA EVENT TO FLASK
// =====================================================

void sendLoraEventToFlask(
  const String &command
)
{
  if (
    WiFi.status() !=
    WL_CONNECTED
  )
  {
    return;
  }

  HTTPClient http;

  http.setConnectTimeout(
    1200
  );

  http.setTimeout(
    1500
  );

  String url =
    String(
      FLASK_BASE_URL
    )
    +
    "/api/lora/event";

  http.begin(url);

  http.addHeader(
    "Content-Type",
    "application/json"
  );

  String body =
    "{\"command\":\""
    + command
    + "\",\"rssi\":"
    + String(lastLoraRssi)
    + ",\"state\":"
    + makeStateJson()
    + "}";

  int code =
    http.POST(body);

  Serial.print(
    "POST LoRa event -> "
  );

  Serial.println(
    code
  );

  http.end();
}

// =====================================================
// SEND DHT11 TO ARDUINO UNO
// =====================================================

void sendDhtToArduino()
{
  if (
    isnan(temperature) ||
    isnan(humidity)
  )
  {
    return;
  }

  String packet =
    "DHT|"
    +
    String(
      temperature,
      1
    )
    +
    "|"
    +
    String(
      humidity,
      1
    );

  Serial.print(
    "Sending DHT11 to UNO: "
  );

  Serial.println(
    packet
  );

  // Stop RX
  LoRa.idle();

  delay(10);

  // Send packet
  LoRa.beginPacket();

  LoRa.print(
    packet
  );

  int result =
    LoRa.endPacket();

  Serial.print(
    "DHT packet result: "
  );

  Serial.println(
    result
  );

  delay(20);

  // Return to RX
  LoRa.receive();
}

// =====================================================
// EXECUTE COMMAND
// =====================================================

String executeCommand(
  String command
)
{
  command.trim();

  // ===================================================
  // ROOM LIGHT
  // ===================================================

  if (
    command ==
    "LIGHT|ON"
  )
  {
    setRoomLight(
      true
    );

    return "LIGHT ON";
  }

  if (
    command ==
    "LIGHT|OFF"
  )
  {
    setRoomLight(
      false
    );

    return "LIGHT OFF";
  }

  if (
    command ==
    "LIGHT|TOGGLE"
  )
  {
    setRoomLight(
      !roomLight
    );

    return roomLight
      ? "LIGHT ON"
      : "LIGHT OFF";
  }

  // ===================================================
  // FAN
  // ===================================================

  if (
    command ==
    "FAN|ON"
  )
  {
    setFan(true);

    return "FAN ON";
  }

  if (
    command ==
    "FAN|OFF"
  )
  {
    setFan(false);

    return "FAN OFF";
  }

  if (
    command ==
    "FAN|TOGGLE"
  )
  {
    setFan(
      !fanOn
    );

    return fanOn
      ? "FAN ON"
      : "FAN OFF";
  }

  // ===================================================
  // OUTDOOR AUTO
  // ===================================================

  if (
    command ==
    "OUTDOOR|AUTO"
  )
  {
    outdoorMode =
      "auto";

    updateOutdoorAutomation();

    stateDirty = true;

    return
      "OUTDOOR AUTO";
  }

  // ===================================================
  // OUTDOOR MANUAL
  // ===================================================

  if (
    command ==
    "OUTDOOR|MANUAL"
  )
  {
    outdoorMode =
      "manual";

    stateDirty = true;

    return
      "OUTDOOR MANUAL";
  }

  // ===================================================
  // OUTDOOR ON
  // ===================================================

  if (
    command ==
    "OUTDOOR|ON"
  )
  {
    outdoorMode =
      "manual";

    setOutdoorLight(
      true
    );

    return
      "OUTDOOR ON";
  }

  // ===================================================
  // OUTDOOR OFF
  // ===================================================

  if (
    command ==
    "OUTDOOR|OFF"
  )
  {
    outdoorMode =
      "manual";

    setOutdoorLight(
      false
    );

    return
      "OUTDOOR OFF";
  }

  // ===================================================
  // OUTDOOR TOGGLE
  // ===================================================

  if (
    command ==
    "OUTDOOR|TOGGLE"
  )
  {
    outdoorMode =
      "manual";

    setOutdoorLight(
      !outdoorLight
    );

    return outdoorLight
      ? "OUTDOOR ON"
      : "OUTDOOR OFF";
  }

  // ===================================================
  // DOOR LOCK
  // ===================================================

  if (
    command ==
    "DOOR|LOCK"
  )
  {
    lockDoor();

    return
      "DOOR LOCKED";
  }

  // ===================================================
  // DOOR UNLOCK
  // ===================================================

  if (
    command ==
    "DOOR|UNLOCK"
  )
  {
    unlockDoor();

    return
      "DOOR UNLOCKED";
  }

  // ===================================================
  // DOOR TOGGLE
  // ===================================================

  if (
    command ==
    "DOOR|TOGGLE"
  )
  {
    if (
      doorLocked
    )
    {
      unlockDoor();
    }
    else
    {
      lockDoor();
    }

    return doorLocked
      ? "DOOR LOCKED"
      : "DOOR UNLOCKED";
  }

  return
    "UNKNOWN COMMAND";
}

// =====================================================
// CHECK LORA
// =====================================================

void checkLoRa()
{
  int packetSize =
    LoRa.parsePacket();

  if (
    !packetSize
  )
  {
    return;
  }

  String command = "";

  while (
    LoRa.available()
  )
  {
    command +=
      (char)LoRa.read();
  }

  command.trim();

  lastLoraRssi =
    LoRa.packetRssi();

  lastLoraSnr =
    LoRa.packetSnr();

  loraConnected =
    true;

  Serial.println();

  Serial.println(
    "================================"
  );

  Serial.println(
    "LORA COMMAND FROM UNO"
  );

  Serial.print(
    "Command: "
  );

  Serial.println(
    command
  );

  Serial.print(
    "RSSI: "
  );

  Serial.print(
    lastLoraRssi
  );

  Serial.println(
    " dBm"
  );

  Serial.print(
    "SNR: "
  );

  Serial.println(
    lastLoraSnr
  );

  String result =
    executeCommand(
      command
    );

  Serial.print(
    "Result: "
  );

  Serial.println(
    result
  );

  Serial.println(
    "================================"
  );

  // Return to receive mode
  LoRa.receive();

  // Send activity to Flask
  sendLoraEventToFlask(
    command
  );

  stateDirty = true;
}

// =====================================================
// READ HTTP BODY
// =====================================================

String readRequestBody()
{
  return server.hasArg(
    "plain"
  )
    ? server.arg("plain")
    : "";
}

// =====================================================
// EXTRACT JSON STRING
// =====================================================

String extractJsonString(
  String body,
  String key
)
{
  String pattern =
    "\"" +
    key +
    "\"";

  int keyPos =
    body.indexOf(
      pattern
    );

  if (
    keyPos < 0
  )
  {
    return "";
  }

  int colonPos =
    body.indexOf(
      ':',
      keyPos
    );

  int quote1 =
    body.indexOf(
      '"',
      colonPos + 1
    );

  int quote2 =
    body.indexOf(
      '"',
      quote1 + 1
    );

  if (
    quote1 < 0 ||
    quote2 < 0
  )
  {
    return "";
  }

  return body.substring(
    quote1 + 1,
    quote2
  );
}

// =====================================================
// EXTRACT JSON BOOL
// =====================================================

bool extractJsonBool(
  String body,
  String key,
  bool fallback = false
)
{
  String pattern =
    "\"" +
    key +
    "\"";

  int keyPos =
    body.indexOf(
      pattern
    );

  if (
    keyPos < 0
  )
  {
    return fallback;
  }

  int colonPos =
    body.indexOf(
      ':',
      keyPos
    );

  String rest =
    body.substring(
      colonPos + 1
    );

  rest.trim();

  return rest.startsWith(
    "true"
  );
}

// =====================================================
// WEBSITE CONTROL HANDLER
// =====================================================

void handleControl()
{
  String body =
    readRequestBody();

  String device =
    extractJsonString(
      body,
      "device"
    );

  bool state =
    extractJsonBool(
      body,
      "state"
    );

  // Room light
  if (
    device ==
    "light"
  )
  {
    setRoomLight(
      state
    );
  }

  // Fan
  else if (
    device ==
    "fan"
  )
  {
    setFan(
      state
    );
  }

  // Outdoor
  else if (
    device ==
    "outdoorLight"
  )
  {
    // Prevent manual switch
    // while AUTO mode active

    if (
      outdoorMode ==
      "auto"
    )
    {
      server.send(
        409,
        "application/json",
        "{\"success\":false,\"message\":\"Outdoor light is in auto mode\"}"
      );

      return;
    }

    setOutdoorLight(
      state
    );
  }

  // Door
  else if (
    device ==
    "door"
  )
  {
    if (
      state
    )
    {
      unlockDoor();
    }
    else
    {
      lockDoor();
    }
  }

  // Invalid
  else
  {
    server.send(
      400,
      "application/json",
      "{\"success\":false,\"message\":\"Invalid device\"}"
    );

    return;
  }

  stateDirty = true;

  server.send(
    200,
    "application/json",
    "{\"success\":true}"
  );
}

// =====================================================
// OUTDOOR MODE HANDLER
// =====================================================

void handleOutdoorMode()
{
  String body =
    readRequestBody();

  String mode =
    extractJsonString(
      body,
      "mode"
    );

  if (
    mode != "auto" &&
    mode != "manual"
  )
  {
    server.send(
      400,
      "application/json",
      "{\"success\":false,\"message\":\"Invalid mode\"}"
    );

    return;
  }

  outdoorMode =
    mode;

  // When AUTO is selected,
  // immediately apply LDR logic

  if (
    mode ==
    "auto"
  )
  {
    updateOutdoorAutomation();
  }

  Serial.print(
    "Outdoor Mode: "
  );

  Serial.println(
    outdoorMode
  );

  stateDirty = true;

  server.send(
    200,
    "application/json",
    "{\"success\":true}"
  );
}

// =====================================================
// ESP32 HTTP SERVER
// =====================================================

void setupHttpServer()
{
  // Control devices
  server.on(
    "/api/control",
    HTTP_POST,
    handleControl
  );

  // Outdoor mode
  server.on(
    "/api/outdoor/mode",
    HTTP_POST,
    handleOutdoorMode
  );

  // ESP32 status
  server.on(
    "/api/status",
    HTTP_GET,
    []()
    {
      server.send(
        200,
        "application/json",
        makeStateJson()
      );
    }
  );

  server.begin();

  Serial.println(
    "ESP32 HTTP server started on port 80"
  );
}

// =====================================================
// CONNECT WIFI
// =====================================================

void connectWiFi()
{
  if (
    WiFi.status() ==
    WL_CONNECTED
  )
  {
    return;
  }

  Serial.print(
    "Connecting to Wi-Fi"
  );

  WiFi.begin(
    WIFI_SSID,
    WIFI_PASSWORD
  );

  unsigned long start =
    millis();

  while (
    WiFi.status() !=
      WL_CONNECTED
    &&
    millis() - start <
      15000
  )
  {
    delay(500);

    Serial.print(".");
  }

  Serial.println();

  if (
    WiFi.status() ==
    WL_CONNECTED
  )
  {
    Serial.print(
      "ESP32 IP: "
    );

    Serial.println(
      WiFi.localIP()
    );
  }
  else
  {
    Serial.println(
      "Wi-Fi not connected. LoRa still works."
    );
  }
}

// =====================================================
// SETUP
// =====================================================

void setup()
{
  Serial.begin(
    115200
  );

  delay(1000);

  Serial.println();

  Serial.println(
    "================================"
  );

  Serial.println(
    "ESP32 SMART HOME"
  );

  Serial.println(
    "LDR > 3000 = DARK = OUTDOOR ON"
  );

  Serial.println(
    "LDR <= 3000 = BRIGHT = OUTDOOR OFF"
  );

  Serial.println(
    "================================"
  );

  // ===================================================
  // OUTPUTS
  // ===================================================

  pinMode(
    LIGHT_PIN,
    OUTPUT
  );

  pinMode(
    FAN_PIN,
    OUTPUT
  );

  pinMode(
    OUTDOOR_PIN,
    OUTPUT
  );

  digitalWrite(
    LIGHT_PIN,
    LOW
  );

  digitalWrite(
    FAN_PIN,
    LOW
  );

  digitalWrite(
    OUTDOOR_PIN,
    LOW
  );

  // ===================================================
  // DHT11
  // ===================================================

  dht.begin();

  // ===================================================
  // SERVO
  // ===================================================

  doorServo.setPeriodHertz(
    50
  );

  doorServo.attach(
    SERVO_PIN,
    500,
    2400
  );

  lockDoor();

  // ===================================================
  // WIFI
  // ===================================================

  connectWiFi();

  // ===================================================
  // SPI + LORA
  // ===================================================

  SPI.begin(
    LORA_SCK,
    LORA_MISO,
    LORA_MOSI,
    LORA_SS
  );

  LoRa.setPins(
    LORA_SS,
    LORA_RST,
    LORA_DIO0
  );

  if (
    !LoRa.begin(
      LORA_FREQUENCY
    )
  )
  {
    Serial.println(
      "ERROR: LoRa initialization failed!"
    );

    while (true)
    {
      delay(1000);
    }
  }

  // ===================================================
  // RADIO SETTINGS
  // ===================================================

  LoRa.setSpreadingFactor(
    7
  );

  LoRa.setSignalBandwidth(
    125E3
  );

  LoRa.setCodingRate4(
    5
  );

  LoRa.setSyncWord(
    0x12
  );

  LoRa.enableCrc();

  LoRa.receive();

  Serial.println(
    "LoRa ready at 433 MHz"
  );

  // ===================================================
  // HTTP SERVER
  // ===================================================

  setupHttpServer();

  // ===================================================
  // FIRST SENSOR READ
  // ===================================================

  delay(2000);

  readSensors();

  // ===================================================
  // FIRST FLASK UPDATE
  // ===================================================

  sendStateToFlask();
}

// =====================================================
// LOOP
// =====================================================

void loop()
{
  // ===================================================
  // WEBSITE HTTP REQUESTS
  // ===================================================

  server.handleClient();

  // ===================================================
  // LORA COMMANDS
  // ===================================================

  checkLoRa();

  unsigned long now =
    millis();

  // ===================================================
  // SENSOR READ EVERY 2 SECONDS
  // ===================================================

  if (
    now - lastSensorRead
    >= SENSOR_INTERVAL
  )
  {
    lastSensorRead =
      now;

    readSensors();
  }

  // ===================================================
  // SEND DHT TO UNO EVERY 5 SECONDS
  // ===================================================

  if (
    now - lastDhtLoRaSend
    >= DHT_LORA_INTERVAL
  )
  {
    lastDhtLoRaSend =
      now;

    sendDhtToArduino();
  }

  // ===================================================
  // WIFI RECONNECT
  // ===================================================

  if (
    WiFi.status() !=
      WL_CONNECTED
    &&
    now - lastWiFiRetry
      >= WIFI_RETRY_INTERVAL
  )
  {
    lastWiFiRetry =
      now;

    WiFi.disconnect();

    WiFi.begin(
      WIFI_SSID,
      WIFI_PASSWORD
    );
  }

  // ===================================================
  // SEND STATUS TO FLASK
  // ===================================================

  if (
    now - lastFlaskPost
      >= FLASK_INTERVAL
    ||
    stateDirty
  )
  {
    lastFlaskPost =
      now;

    sendStateToFlask();
  }
}