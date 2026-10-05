export default function SensorBadge({ icon: Icon, value, label, className = "", tone = "blue" }) {
  return (
    <div className={`sensor-badge ${className}`}>
      <span className={`sensor-badge-icon ${tone}`}>
        <Icon size={21} />
      </span>
      <div>
        <strong>{value}</strong>
        <span>{label}</span>
      </div>
    </div>
  );
}
