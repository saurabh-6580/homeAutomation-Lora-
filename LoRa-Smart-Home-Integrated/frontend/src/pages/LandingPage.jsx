import { useEffect, useState } from "react";
import { Link } from "react-router-dom";
import {
  Activity,
  Droplets,
  Fan,
  Home,
  LayoutDashboard,
  Leaf,
  Lock,
  Radio,
  ShieldCheck,
  Sun,
  Thermometer,
  Wifi
} from "lucide-react";

import Navbar from "../components/Navbar";
import SensorBadge from "../components/SensorBadge";
import BenefitCard from "../components/BenefitCard";
import houseImage from "../assets/smart-home-house.png";

const fallback = {
  temp: 27.2,
  humidity: 65,
  light: true,
  fan: true,
  outdoorLight: false,
  outdoorMode: "auto",
  doorLocked: true,
  loraConnected: true
};

export default function LandingPage() {
  const [status, setStatus] = useState(fallback);

  useEffect(() => {
    let mounted = true;

    const load = async () => {
      try {
        const response = await fetch("/api/status");
        const data = await response.json();
        if (mounted && data.current) {
          setStatus(prev => ({ ...prev, ...data.current }));
        }
      } catch {
        // Landing page gracefully keeps preview values if backend is not running.
      }
    };

    load();
    const timer = setInterval(load, 5000);

    return () => {
      mounted = false;
      clearInterval(timer);
    };
  }, []);

  return (
    <div className="landing-page">
      <Navbar />

      <main className="hero">
        <section className="hero-copy">
          <div className="technology-pill">
            <Wifi size={14} />
            Hybrid Wi-Fi + LoRa Technology
          </div>

          <h1>
            Smart Home Automation
            <br />
            that Works <span>Everywhere</span>
          </h1>

          <p className="hero-description">
            Monitor your home environment and control appliances using Wi-Fi
            from anywhere on your local network, or LoRa when Wi-Fi is unavailable.
          </p>

          <div className="hero-buttons">
            <Link to="/" className="hero-btn primary">
              <Home size={18} />
              Home
            </Link>

            <Link to="/dashboard" className="hero-btn secondary">
              <LayoutDashboard size={18} />
              Dashboard
            </Link>
          </div>

          <div className="benefit-row">
            <BenefitCard icon={Wifi} title="Hybrid Control" subtitle="Wi-Fi + LoRa" tone="blue" />
            <BenefitCard icon={Leaf} title="Energy Efficient" subtitle="Save Energy" tone="green" />
            <BenefitCard icon={ShieldCheck} title="Secure & Reliable" subtitle="Always Protected" tone="orange" />
            <BenefitCard icon={Activity} title="Real-time Monitor" subtitle="Live Data & Alerts" tone="purple" />
          </div>
        </section>

        <section className="hero-visual">
          <div className="house-stage">
            <div className="soft-orb orb-a" />
            <div className="soft-orb orb-b" />

            <img
              src={houseImage}
              className="house-image"
              alt="Isometric smart home"
            />

            <SensorBadge
              icon={Thermometer}
              value={`${status.temp ?? "--"} °C`}
              label="Temperature"
              className="badge-temp"
              tone="blue"
            />

            <SensorBadge
              icon={Droplets}
              value={`${status.humidity ?? "--"} %`}
              label="Humidity"
              className="badge-humidity"
              tone="blue"
            />

            <SensorBadge
              icon={Fan}
              value={status.fan ? "ON" : "OFF"}
              label="Smart Fan"
              className="badge-fan"
              tone="green"
            />

            <SensorBadge
              icon={Radio}
              value="LoRa Remote"
              label="Arduino Uno"
              className="badge-lora"
              tone="green"
            />

            <SensorBadge
              icon={Lock}
              value={status.doorLocked ? "Door Locked" : "Door Unlocked"}
              label={status.doorLocked ? "Secure" : "Open"}
              className="badge-door"
              tone="orange"
            />

            <SensorBadge
              icon={Sun}
              value="Outdoor Light"
              label={status.outdoorMode === "auto" ? "Auto Mode" : "Manual Mode"}
              className="badge-outdoor"
              tone="orange"
            />

            <span className="connector connector-temp" />
            <span className="connector connector-humidity green-line" />
            <span className="connector connector-fan green-line" />
            <span className="connector connector-lora green-line" />
            <span className="connector connector-door orange-line" />
            <span className="connector connector-outdoor orange-line" />
          </div>
        </section>
      </main>
    </div>
  );
}
