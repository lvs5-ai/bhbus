interface MetricCardProps {
  label: string;
  value: string | number;
  compact?: boolean;
}

export function MetricCard({ label, value, compact = false }: MetricCardProps) {
  return (
    <article className="card highlight">
      <span className="label">{label}</span>
      <strong className={`big-value${compact ? ' small' : ''}`}>{value}</strong>
    </article>
  );
}
