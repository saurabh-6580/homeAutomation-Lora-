from flask import Flask, request, jsonify, Response
from flask_cors import CORS
from collections import deque
from threading import Lock
from datetime import datetime

app = Flask(__name__)
CORS(app)

# =========================================================
# CURRENT ESP32 STATE
# =========================================================
current_state = {
    "temp": None,
    "humidity": None,
    "ldr": 0,
    "environment": "Unknown",
    "light": False,
    "fan": False,
    "outdoorLight": False,
    "outdoorMode": "auto",
    "doorLocked": True,
    "wifiConnected": False,
    "loraConnected": False,
    "loraRssi": None,
}

# =========================================================
# DASHBOARD HISTORY / ACTIVITY
# =========================================================
MAX_HISTORY = 30
MAX_ACTIVITY = 30

history = {
    "labels": deque(maxlen=MAX_HISTORY),
    "temperature": deque(maxlen=MAX_HISTORY),
    "humidity": deque(maxlen=MAX_HISTORY),
}

activity = deque(maxlen=MAX_ACTIVITY)

# =========================================================
# COMMAND QUEUE
# React -> Flask stores command here
# ESP32 -> polls /api/iot/command and receives it
# =========================================================
pending_commands = deque(maxlen=50)
command_lock = Lock()


def add_activity(message, source="System"):
    activity.append({
        "message": message,
        "source": source,
        "time": datetime.now().strftime("%H:%M:%S"),
    })


def parse_bool(value):
    if isinstance(value, bool):
        return value
    if isinstance(value, (int, float)):
        return value != 0
    if isinstance(value, str):
        return value.strip().lower() in ("true", "1", "on", "yes")
    return False


# =========================================================
# HEALTH
# =========================================================
@app.route("/api/health", methods=["GET"])
def health():
    with command_lock:
        queued = len(pending_commands)

    return jsonify({
        "status": "ok",
        "hardwareMode": True,
        "controlMode": "esp32-polling",
        "pendingCommands": queued,
    })


# =========================================================
# DASHBOARD STATUS
# =========================================================
@app.route("/api/status", methods=["GET"])
def status():
    return jsonify({
        "current": current_state,
        "activity": list(activity),
        "history": {
            "labels": list(history["labels"]),
            "temperature": list(history["temperature"]),
            "humidity": list(history["humidity"]),
        },
        "date": datetime.now().strftime("%d %b %Y"),
        "time": datetime.now().strftime("%H:%M:%S"),
    })


# =========================================================
# ESP32 POSTS SENSOR + DEVICE STATE HERE
# =========================================================
@app.route("/api/iot/status", methods=["POST"])
def receive_esp32_status():
    data = request.get_json(silent=True)

    if not isinstance(data, dict):
        return jsonify({
            "success": False,
            "message": "Valid JSON body required",
        }), 400

    old_state = dict(current_state)

    allowed_keys = {
        "temp",
        "humidity",
        "ldr",
        "environment",
        "light",
        "fan",
        "outdoorLight",
        "outdoorMode",
        "doorLocked",
        "wifiConnected",
        "loraConnected",
        "loraRssi",
    }

    for key in allowed_keys:
        if key in data:
            current_state[key] = data[key]

    # Keep small sensor history for dashboard charts.
    now_label = datetime.now().strftime("%H:%M:%S")
    history["labels"].append(now_label)
    history["temperature"].append(current_state.get("temp"))
    history["humidity"].append(current_state.get("humidity"))

    # Log only meaningful state changes instead of logging every sensor POST.
    if old_state.get("light") != current_state.get("light"):
        add_activity(
            "Room Light ON" if current_state["light"] else "Room Light OFF",
            "ESP32",
        )

    if old_state.get("fan") != current_state.get("fan"):
        add_activity(
            "Fan ON" if current_state["fan"] else "Fan OFF",
            "ESP32",
        )

    if old_state.get("outdoorLight") != current_state.get("outdoorLight"):
        add_activity(
            "Outdoor Light ON" if current_state["outdoorLight"] else "Outdoor Light OFF",
            "ESP32",
        )

    if old_state.get("outdoorMode") != current_state.get("outdoorMode"):
        add_activity(
            f"Outdoor Mode: {str(current_state['outdoorMode']).upper()}",
            "ESP32",
        )

    if old_state.get("doorLocked") != current_state.get("doorLocked"):
        add_activity(
            "Door LOCKED" if current_state["doorLocked"] else "Door UNLOCKED",
            "ESP32",
        )

    if old_state.get("environment") != current_state.get("environment"):
        add_activity(
            f"Environment: {current_state['environment']}",
            "ESP32 Sensor",
        )

    return jsonify({
        "success": True,
        "message": "ESP32 status received",
    })


# =========================================================
# WEBSITE DEVICE CONTROL
# =========================================================
@app.route("/api/control", methods=["POST"])
def control():
    data = request.get_json(silent=True)

    if not isinstance(data, dict):
        return jsonify({
            "success": False,
            "message": "Valid JSON body required",
        }), 400

    device = data.get("device")

    if device not in ("light", "fan", "outdoorLight", "door"):
        return jsonify({
            "success": False,
            "message": "Invalid device",
        }), 400

    if "state" not in data:
        return jsonify({
            "success": False,
            "message": "state is required",
        }), 400

    state = parse_bool(data.get("state"))

    # Outdoor manual control should only be used in manual mode.
    if device == "outdoorLight" and current_state.get("outdoorMode") == "auto":
        return jsonify({
            "success": False,
            "message": "Outdoor light is in auto mode. Select Manual first.",
        }), 409

    with command_lock:
        pending_commands.append({
            "type": "control",
            "device": device,
            "state": state,
        })

    if device == "door":
        readable = "UNLOCK" if state else "LOCK"
    else:
        readable = "ON" if state else "OFF"

    add_activity(
        f"{device} {readable} command queued",
        "Website",
    )

    return jsonify({
        "success": True,
        "queued": True,
        "message": "Command queued for ESP32",
    })


# =========================================================
# WEBSITE OUTDOOR AUTO/MANUAL MODE
# =========================================================
@app.route("/api/outdoor/mode", methods=["POST"])
def outdoor_mode():
    data = request.get_json(silent=True)

    if not isinstance(data, dict):
        return jsonify({
            "success": False,
            "message": "Valid JSON body required",
        }), 400

    mode = str(data.get("mode", "")).strip().lower()

    if mode not in ("auto", "manual"):
        return jsonify({
            "success": False,
            "message": "mode must be auto or manual",
        }), 400

    with command_lock:
        pending_commands.append({
            "type": "outdoor_mode",
            "mode": mode,
        })

    add_activity(
        f"Outdoor mode {mode.upper()} command queued",
        "Website",
    )

    return jsonify({
        "success": True,
        "queued": True,
        "message": f"Outdoor {mode} command queued for ESP32",
    })


# =========================================================
# ESP32 POLLS THIS ENDPOINT
# Returns plain text so ESP32 does not need ArduinoJson.
# =========================================================
@app.route("/api/iot/command", methods=["GET"])
def get_pending_command():
    with command_lock:
        if not pending_commands:
            return Response("NONE", mimetype="text/plain")

        command = pending_commands.popleft()

    if command["type"] == "control":
        state_value = "1" if command["state"] else "0"
        payload = f"CONTROL|{command['device']}|{state_value}"
        return Response(payload, mimetype="text/plain")

    if command["type"] == "outdoor_mode":
        payload = f"MODE|{command['mode']}"
        return Response(payload, mimetype="text/plain")

    return Response("NONE", mimetype="text/plain")


# =========================================================
# ESP32 REPORTS LORA EVENTS HERE
# =========================================================
@app.route("/api/lora/event", methods=["POST"])
def lora_event():
    data = request.get_json(silent=True) or {}

    command = str(data.get("command", "LoRa command received"))

    if "rssi" in data:
        current_state["loraRssi"] = data.get("rssi")
        current_state["loraConnected"] = True

    state = data.get("state")
    if isinstance(state, dict):
        for key in current_state:
            if key in state:
                current_state[key] = state[key]

    add_activity(command, "Arduino LoRa")

    return jsonify({
        "success": True,
    })


# =========================================================
# START FLASK
# =========================================================
if __name__ == "__main__":
    print("Smart Home Flask backend starting...")
    print("Control mode: ESP32 polling")
    print("ESP32 should poll: /api/iot/command")

    # use_reloader=False is important because the pending command queue
    # is stored in memory. The Flask reloader can create a second process.
    app.run(
        host="0.0.0.0",
        port=5000,
        debug=True,
        use_reloader=False,
    )
