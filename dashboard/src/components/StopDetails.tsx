import type { Snapshot } from '../types/snapshot';

interface StopDetailsProps {
  snapshot: Snapshot | null;
}

export function StopDetails({ snapshot }: StopDetailsProps) {
  return (
    <article className="card">
      <h2>Parada monitorada</h2>
      <div className="kv-list">
        <div><span>Latitude</span><strong>{snapshot?.stop.lat ?? '--'}</strong></div>
        <div><span>Longitude</span><strong>{snapshot?.stop.lon ?? '--'}</strong></div>
        <div><span>Sentido filtrado</span><strong>{snapshot?.trip_direction_filter ?? 'todos'}</strong></div>
        <div>
          <span>Previsão principal</span>
          <strong>{snapshot?.best_prediction.vehicle_id ?? '--'}</strong>
        </div>
        <div><span>Recebido no servidor</span><strong>{snapshot?.received_at ?? '--'}</strong></div>
      </div>
    </article>
  );
}
