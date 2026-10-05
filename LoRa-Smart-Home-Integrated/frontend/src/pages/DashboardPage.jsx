import { useEffect, useMemo, useState } from "react";
import { Link, NavLink } from "react-router-dom";

import {
  Activity,
  CalendarDays,
  Clock3,
  Droplets,
  Fan,
  House,
  Lightbulb,
  Lock,
  Radio,
  Sun,
  Thermometer,
  Unlock,
  Wifi,
} from "lucide-react";

import {
  Area,
  AreaChart,
  CartesianGrid,
  Legend,
  ResponsiveContainer,
  Tooltip,
  XAxis,
  YAxis,
} from "recharts";

/* =========================================================
   API HELPER
========================================================= */

async function api(path, options = {}) {
  const response = await fetch(`/api${path}`, {
    headers: {
      "Content-Type": "application/json",
    },
    ...options,
  });

  const body = await response.json();

  if (!response.ok) {
    throw new Error(body.message || "Request failed");
  }

  return body;
}

/* =========================================================
   TOGGLE SWITCH
========================================================= */

function Toggle({ on, disabled = false, onClick }) {
  return (
    <button
      className={`toggle ${on ? "on" : ""}`}
      disabled={disabled}
      onClick={onClick}
      aria-label={on ? "Turn off" : "Turn on"}
    >
      <span />
    </button>
  );
}

/* =========================================================
   DEVICE CARD
========================================================= */

function DeviceCard({
  icon: Icon,
  title,
  subtitle,
  on,
  disabled = false,
  onToggle,
  tone = "blue",
}) {
  return (
    <article className={`device-card ${on ? "active" : ""}`}>
      <div className="device-top">
        <span className={`device-icon ${tone}`}>
          <Icon size={26} />
        </span>

        <Toggle on={on} disabled={disabled} onClick={onToggle} />
      </div>

      <h3>{title}</h3>

      <p>{subtitle}</p>

      <div className="device-state-row">
        <span className={`device-state-dot ${on ? "on" : ""}`} />

        <span className={on ? "device-on" : "device-off"}>
          {on ? "ON" : "OFF"}
        </span>
      </div>
    </article>
  );
}

/* =========================================================
   DASHBOARD PAGE
========================================================= */

export default function DashboardPage() {
  const [payload, setPayload] = useState(null);

  const [error, setError] = useState("");

  const [loadingDevice, setLoadingDevice] = useState("");

  /* =======================================================
     LOAD STATUS
  ======================================================= */

  const loadStatus = async () => {
    try {
      const data = await api("/status");

      setPayload(data);

      setError("");
    } catch (err) {
      setError(err.message);
    }
  };

  /* =======================================================
     AUTO REFRESH
  ======================================================= */

  useEffect(() => {
    loadStatus();

    const timer = setInterval(loadStatus, 2500);

    return () => clearInterval(timer);
  }, []);

  const status = payload?.current || {};

  /* =======================================================
     CHART DATA
  ======================================================= */

  const chartData = useMemo(() => {
    if (!payload?.history) {
      return [];
    }

    return payload.history.labels.map((time, index) => ({
      time,

      temperature: payload.history.temperature[index],

      humidity: payload.history.humidity[index],
    }));
  }, [payload]);

  /* =======================================================
     DEVICE CONTROL
  ======================================================= */

  const control = async (device, state) => {
    try {
      setLoadingDevice(device);

      await api("/control", {
        method: "POST",

        body: JSON.stringify({
          device,
          state,
        }),
      });

      await loadStatus();
    } catch (err) {
      setError(err.message);
    } finally {
      setLoadingDevice("");
    }
  };

  /* =======================================================
     OUTDOOR MODE
  ======================================================= */

  const setOutdoorMode = async (mode) => {
    try {
      setLoadingDevice("outdoorMode");

      await api("/outdoor/mode", {
        method: "POST",

        body: JSON.stringify({
          mode,
        }),
      });

      await loadStatus();
    } catch (err) {
      setError(err.message);
    } finally {
      setLoadingDevice("");
    }
  };

  return (
    <div className="dashboard-page">
      {/* ===================================================
          NAVBAR
      =================================================== */}

      <header className="dashboard-navbar">
        <Link to="/" className="brand">
          <span className="brand-icon">
            <House size={24} />
          </span>

          <span className="brand-text">
            LoRa<span>Nest</span>
          </span>
        </Link>

        {/* SAME NAVIGATION AS LANDING PAGE */}

        <div className="nav-links">
          <NavLink
            to="/"
            end
            className={({ isActive }) => (isActive ? "active" : "")}
          >
            Home
          </NavLink>

          <NavLink
            to="/dashboard"
            className={({ isActive }) => (isActive ? "active" : "")}
          >
            Dashboard
          </NavLink>
        </div>
      </header>

      {/* ===================================================
          DASHBOARD CONTENT
      =================================================== */}

      <main className="dashboard-content">
        {/* =================================================
            DASHBOARD HEADER
        ================================================= */}

        <section className="dashboard-header">
          <div className="dashboard-heading">
            <span className="dashboard-kicker">SMART HOME CONTROL CENTER</span>

            <h1>Smart Home Dashboard</h1>

            <p>
              Monitor your environment, control appliances and check your LoRa
              system in real-time.
            </p>
          </div>

          {/* SYSTEM INFORMATION */}

          <div className="system-information">
            <div className="connection-pills">
              <span className="status-pill success">
                <span className="live-dot" />
                System Online
              </span>

              <span
                className={`status-pill ${
                  status.wifiConnected ? "info" : "muted"
                }`}
              >
                <Wifi size={15} />
                Wi-Fi {status.wifiConnected ? "Connected" : "Offline"}
              </span>

              <span
                className={`status-pill ${
                  status.loraConnected ? "purple" : "muted"
                }`}
              >
                <Radio size={15} />
                LoRa {status.loraConnected ? "Connected" : "Waiting"}
              </span>
            </div>

            <div className="dashboard-date">
              <span>
                <CalendarDays size={16} />

                {payload?.date || "--"}
              </span>

              <span>
                <Clock3 size={16} />

                {payload?.time || "--"}
              </span>
            </div>
          </div>
        </section>

        {/* ERROR */}

        {error && <div className="error-banner">{error}</div>}

        {/* =================================================
            SENSOR METRICS
        ================================================= */}

        <section className="metric-grid">
          {/* TEMPERATURE */}

          <article className="metric-card">
            <span className="metric-icon temp">
              <Thermometer size={30} />
            </span>

            <div className="metric-information">
              <span className="metric-label">Temperature</span>

              <strong>
                {status.temp ?? "--"}

                <small>°C</small>
              </strong>

              <p>Indoor Sensor</p>
            </div>

            <div className="mini-wave temperature-wave">∿∿∿</div>
          </article>

          {/* HUMIDITY */}

          <article className="metric-card">
            <span className="metric-icon humidity">
              <Droplets size={30} />
            </span>

            <div className="metric-information">
              <span className="metric-label">Humidity</span>

              <strong>
                {status.humidity ?? "--"}

                <small>%</small>
              </strong>

              <p>Indoor Sensor</p>
            </div>

            <div className="mini-wave humidity-wave">∿∿∿</div>
          </article>

          {/* LDR */}

          <article className="metric-card">
            <span className="metric-icon light">
              <Sun size={30} />
            </span>

            <div className="metric-information">
              <span className="metric-label">Ambient Light</span>

              <strong>{status.ldr ?? "--"}</strong>

              <p className="good-text">{status.environment || "Waiting"}</p>
            </div>

            <div className="mini-wave light-wave">∿∿∿</div>
          </article>

          {/* LORA */}

          <article className="metric-card">
            <span className="metric-icon signal">
              <Radio size={30} />
            </span>

            <div className="metric-information">
              <span className="metric-label">LoRa Signal</span>

              <strong>
                {status.loraRssi ?? "--"}

                <small>dBm</small>
              </strong>

              <p className={status.loraConnected ? "good-text" : ""}>
                {status.loraConnected ? "Good signal" : "No packet"}
              </p>
            </div>

            <div className="mini-wave signal-wave">∿∿∿</div>
          </article>
        </section>

        {/* =================================================
            DEVICE + AUTOMATION ROW
        ================================================= */}

        <section className="control-layout">
          {/* DEVICE CONTROL */}

          <article className="panel controls-panel">
            <div className="panel-header">
              <div>
                <h2>Devices Control</h2>

                <p>Control your home appliances remotely.</p>
              </div>

              <span className="panel-count">
                {
                  [status.light, status.fan, status.outdoorLight].filter(
                    Boolean,
                  ).length
                }{" "}
                active
              </span>
            </div>

            <div className="device-grid">
              <DeviceCard
                icon={Lightbulb}
                title="Room Light"
                subtitle="Living area"
                tone="green"
                on={Boolean(status.light)}
                disabled={loadingDevice === "light"}
                onToggle={() => control("light", !status.light)}
              />

              <DeviceCard
                icon={Fan}
                title="Ceiling Fan"
                subtitle="Indoor fan"
                tone="blue"
                on={Boolean(status.fan)}
                disabled={loadingDevice === "fan"}
                onToggle={() => control("fan", !status.fan)}
              />

              <DeviceCard
                icon={Sun}
                title="Outdoor Light"
                subtitle={
                  status.outdoorMode === "auto"
                    ? "LDR automatic mode"
                    : "Manual control"
                }
                tone="orange"
                on={Boolean(status.outdoorLight)}
                disabled={
                  status.outdoorMode === "auto" ||
                  loadingDevice === "outdoorLight"
                }
                onToggle={() => control("outdoorLight", !status.outdoorLight)}
              />

              {/* DOOR */}

              <article
                className={`device-card ${!status.doorLocked ? "active" : ""}`}
              >
                <div className="device-top">
                  <span className="device-icon orange">
                    {status.doorLocked ? (
                      <Lock size={26} />
                    ) : (
                      <Unlock size={26} />
                    )}
                  </span>

                  <span
                    className={
                      status.doorLocked
                        ? "door-status locked"
                        : "door-status unlocked"
                    }
                  >
                    {status.doorLocked ? "LOCKED" : "UNLOCKED"}
                  </span>
                </div>

                <h3>Door Lock</h3>

                <p>Servo access control</p>

                <button
                  className="door-button"
                  disabled={loadingDevice === "door"}
                  onClick={() => control("door", status.doorLocked)}
                >
                  {status.doorLocked ? (
                    <Unlock size={17} />
                  ) : (
                    <Lock size={17} />
                  )}

                  {status.doorLocked ? "Unlock Door" : "Lock Door"}
                </button>
              </article>
            </div>
          </article>

          {/* =================================================
              OUTDOOR AUTOMATION
          ================================================= */}

          <article className="panel automation-panel">
            <div className="panel-header">
              <div>
                <h2>Outdoor Light Automation</h2>

                <p>LDR based smart lighting.</p>
              </div>
            </div>

            <div className="mode-switch">
              <button
                className={status.outdoorMode === "auto" ? "active" : ""}
                onClick={() => setOutdoorMode("auto")}
              >
                Automatic
              </button>

              <button
                className={status.outdoorMode === "manual" ? "active" : ""}
                onClick={() => setOutdoorMode("manual")}
              >
                Manual
              </button>
            </div>

            <div className="auto-visual">
              <span className="sun-circle">
                <Sun size={44} />
              </span>

              <span className="environment-label">Environment</span>

              <strong>{status.environment || "Waiting"}</strong>

              <p>
                LDR Value: <b>{status.ldr ?? "--"}</b>
              </p>

              <div className="automation-description">
                {status.outdoorMode === "auto" ? (
                  <>
                    Outdoor light is automatically controlled by the LDR sensor.
                  </>
                ) : (
                  <>
                    Automatic LDR control is disabled. Use the Outdoor Light
                    switch.
                  </>
                )}
              </div>
            </div>
          </article>
        </section>

        {/* =================================================
            CHART
        ================================================= */}

        <article className="panel chart-panel">
          <div className="panel-header">
            <div>
              <h2>Environment History</h2>

              <p>Recent temperature and humidity readings.</p>
            </div>
          </div>

          <div className="chart-box">
            <ResponsiveContainer width="100%" height="100%">
              <AreaChart
                data={chartData}
                margin={{
                  top: 20,
                  right: 20,
                  left: 0,
                  bottom: 5,
                }}
              >
                <defs>
                  <linearGradient
                    id="temperatureGradient"
                    x1="0"
                    y1="0"
                    x2="0"
                    y2="1"
                  >
                    <stop offset="5%" stopColor="#ff8b3d" stopOpacity={0.25} />

                    <stop offset="95%" stopColor="#ff8b3d" stopOpacity={0} />
                  </linearGradient>

                  <linearGradient
                    id="humidityGradient"
                    x1="0"
                    y1="0"
                    x2="0"
                    y2="1"
                  >
                    <stop offset="5%" stopColor="#3478f6" stopOpacity={0.22} />

                    <stop offset="95%" stopColor="#3478f6" stopOpacity={0} />
                  </linearGradient>
                </defs>

                <CartesianGrid strokeDasharray="4 4" stroke="#e8eef6" />

                <XAxis
                  dataKey="time"
                  tick={{
                    fontSize: 12,
                  }}
                  stroke="#8795a8"
                />

                <YAxis
                  tick={{
                    fontSize: 12,
                  }}
                  stroke="#8795a8"
                />

                <Tooltip
                  contentStyle={{
                    borderRadius: "12px",

                    border: "1px solid #e5edf7",

                    boxShadow: "0 12px 30px rgba(30,60,100,.10)",
                  }}
                />

                <Legend
                  wrapperStyle={{
                    fontSize: "12px",
                  }}
                />

                <Area
                  type="monotone"
                  name="Temperature (°C)"
                  dataKey="temperature"
                  stroke="#ff8b3d"
                  strokeWidth={3}
                  fill="url(#temperatureGradient)"
                />

                <Area
                  type="monotone"
                  name="Humidity (%)"
                  dataKey="humidity"
                  stroke="#3478f6"
                  strokeWidth={3}
                  fill="url(#humidityGradient)"
                />
              </AreaChart>
            </ResponsiveContainer>
          </div>
        </article>

        {/* =================================================
            RECENT ACTIVITY
        ================================================= */}

        <article className="panel activity-panel">
          <div className="panel-header">
            <div>
              <h2>Recent Activity</h2>

              <p>Latest web, ESP32 and LoRa events.</p>
            </div>
          </div>

          <div className="activity-row">
            {(payload?.activity || []).slice(0, 5).map((item, index) => (
              <div className="activity-item" key={index}>
                <span className="activity-icon">
                  <Activity size={19} />
                </span>

                <div>
                  <small>{item.time}</small>

                  <strong>{item.message}</strong>

                  <p>Via {item.source}</p>
                </div>
              </div>
            ))}

            {!payload?.activity?.length && (
              <div className="empty-activity">No activity recorded yet.</div>
            )}
          </div>
        </article>
      </main>
    </div>
  );
}
