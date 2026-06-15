interface HeaderProps {
  status: string;
  hasError: boolean;
}

export function Header({ status, hasError }: HeaderProps) {
  return (
    <header className="hero">
      <div>
        <p className="eyebrow">IoT • Dashboard</p>
        <h1>BH-BUS</h1>
        <p className="subtitle">
          Monitoramento da parada com base no sensor virtual e no feed público em tempo real.
        </p>
      </div>
      <div className={`status-pill${hasError ? ' error' : ''}`} role="status">
        {status}
      </div>
    </header>
  );
}
