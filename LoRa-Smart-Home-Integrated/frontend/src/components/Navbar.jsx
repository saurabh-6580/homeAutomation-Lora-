import { Link, NavLink } from "react-router-dom";
import { House } from "lucide-react";

export default function Navbar() {
  return (
    <nav className="navbar">
      {/* Logo */}
      <Link to="/" className="brand">
        <span className="brand-icon">
          <House size={24} />
        </span>

        <span className="brand-text">
          LoRa<span>Nest</span>
        </span>
      </Link>

      {/* Navigation */}
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
    </nav>
  );
}
