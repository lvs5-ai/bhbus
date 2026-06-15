import { formatEta, formatNumber, movementClass } from '../utils/formatters';
import type { Bus } from '../types/snapshot';

interface BusTableProps {
  buses: Bus[];
  isLoading: boolean;
  onRefresh: () => void;
}

export function BusTable({ buses, isLoading, onRefresh }: BusTableProps) {
  return (
    <section className="card">
      <div className="table-header">
        <h2>Veículos mais próximos</h2>
        <button className="refresh-btn" disabled={isLoading} onClick={onRefresh} type="button">
          {isLoading ? 'Atualizando...' : 'Atualizar agora'}
        </button>
      </div>
      <div className="table-wrapper">
        <table>
          <thead>
            <tr>
              <th>Veículo</th>
              <th>Distância</th>
              <th>Velocidade</th>
              <th>Direção</th>
              <th>ETA</th>
              <th>Movimento</th>
            </tr>
          </thead>
          <tbody>
            {buses.length === 0 ? (
              <tr>
                <td colSpan={6} className="empty-state">Nenhum snapshot disponível no momento.</td>
              </tr>
            ) : buses.map((bus) => (
              <tr key={bus.vehicle_id}>
                <td>{bus.vehicle_id || '--'}</td>
                <td>{formatNumber(bus.distance_m, 0)} m</td>
                <td>{formatNumber(bus.speed_kmh, 0)} km/h</td>
                <td>{bus.direction_deg ?? '--'}</td>
                <td>{formatEta(bus.eta_min)}</td>
                <td>
                  <span className={`movement-chip ${movementClass(bus.movement)}`}>
                    {bus.movement || '--'}
                  </span>
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </section>
  );
}
