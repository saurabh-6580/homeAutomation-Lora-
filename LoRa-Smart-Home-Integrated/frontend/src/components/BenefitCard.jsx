export default function BenefitCard({ icon: Icon, title, subtitle, tone }) {
  return (
    <div className="benefit-card">
      <span className={`benefit-icon ${tone}`}>
        <Icon size={22} />
      </span>
      <strong>{title}</strong>
      <small>{subtitle}</small>
    </div>
  );
}
