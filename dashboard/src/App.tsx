import { BusTable } from './components/BusTable';
import { Header } from './components/Header';
import { MetricCard } from './components/MetricCard';
import { OperationalSummary } from './components/OperationalSummary';
import { StopDetails } from './components/StopDetails';
import { useSnapshot } from './hooks/useSnapshot';
import { formatEta } from './utils/formatters';

export default function App() {
  const { snapshot, isLoading, error, refresh } = useSnapshot();
  const hasSnapshot = Boolean(snapshot?.received_at);
  const lineCode = snapshot?.line_code ?? snapshot?.line_ev ?? '--';

  const status = error
    ?? (isLoading ? 'Atualizando...' : hasSnapshot ? 'Dados sincronizados' : 'Aguardando dados');

  return (
    <main className="page">
      <Header status={status} hasError={Boolean(error)} />

      <section className="grid top-grid">
        <MetricCard label="Linha / NL monitorada" value={lineCode} />
        <MetricCard label="Ônibus encontrados" value={snapshot?.buses_found ?? 0} />
        <MetricCard label="Melhor ETA" value={formatEta(snapshot?.best_prediction.eta_min)} />
        <MetricCard label="Última atualização" value={snapshot?.timestamp ?? '--'} compact />
      </section>

      <section className="grid middle-grid">
        <StopDetails snapshot={snapshot} />
        <OperationalSummary snapshot={snapshot} hasError={Boolean(error)} />
      </section>

      <BusTable
        buses={snapshot?.nearest_buses ?? []}
        isLoading={isLoading}
        onRefresh={() => void refresh()}
      />
    </main>
  );
}
