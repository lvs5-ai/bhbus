import { formatEta } from '../utils/formatters';
import type { Snapshot } from '../types/snapshot';

interface OperationalSummaryProps {
  snapshot: Snapshot | null;
  hasError: boolean;
}

export function OperationalSummary({ snapshot, hasError }: OperationalSummaryProps) {
  const lineCode = snapshot?.line_code ?? snapshot?.line_ev ?? 'N/D';
  let summary = 'O dashboard exibirá aqui a estimativa principal assim que o sensor virtual enviar um snapshot.';

  if (hasError) {
    summary = 'Não foi possível consultar o servidor do dashboard neste momento.';
  } else if (snapshot?.received_at && snapshot.buses_found > 0) {
    summary = `A linha/NL ${lineCode} possui ${snapshot.buses_found} veículo(s) detectado(s) no último ciclo. A melhor previsão atual é do veículo ${snapshot.best_prediction.vehicle_id ?? 'N/D'}, com ETA estimado em ${formatEta(snapshot.best_prediction.eta_min)}.`;
  } else if (snapshot?.received_at) {
    summary = 'Nenhum veículo foi encontrado para a linha monitorada no último ciclo do sensor virtual.';
  }

  return (
    <article className="card">
      <h2>Resumo operacional</h2>
      <p className="summary">{summary}</p>
    </article>
  );
}
