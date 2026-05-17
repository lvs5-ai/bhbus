const statusEl = document.getElementById('connectionStatus');
const lineEvEl = document.getElementById('lineEv');
const busesFoundEl = document.getElementById('busesFound');
const bestEtaEl = document.getElementById('bestEta');
const timestampEl = document.getElementById('timestamp');
const stopLatEl = document.getElementById('stopLat');
const stopLonEl = document.getElementById('stopLon');
const bestVehicleEl = document.getElementById('bestVehicle');
const receivedAtEl = document.getElementById('receivedAt');
const summaryTextEl = document.getElementById('summaryText');
const busesTableBody = document.getElementById('busesTableBody');
const refreshBtn = document.getElementById('refreshBtn');

function formatNumber(value, digits = 1) {
  if (value === null || value === undefined || Number.isNaN(Number(value))) return '--';
  return Number(value).toFixed(digits);
}

function formatEta(value) {
  if (value === null || value === undefined) return '--';
  return `${formatNumber(value, 1)} min`;
}

function movementClass(value = '') {
  return value.toLowerCase().replace(/\s+/g, '-');
}

function renderTable(buses = []) {
  if (!buses.length) {
    busesTableBody.innerHTML = `
      <tr>
        <td colspan="6" class="empty-state">Nenhum snapshot disponível no momento.</td>
      </tr>
    `;
    return;
  }

  busesTableBody.innerHTML = buses.map((bus) => `
    <tr>
      <td>${bus.vehicle_id ?? '--'}</td>
      <td>${formatNumber(bus.distance_m, 0)} m</td>
      <td>${formatNumber(bus.speed_kmh, 0)} km/h</td>
      <td>${bus.direction_deg ?? '--'}</td>
      <td>${formatEta(bus.eta_min)}</td>
      <td><span class="movement-chip ${movementClass(bus.movement)}">${bus.movement ?? '--'}</span></td>
    </tr>
  `).join('');
}

function renderSnapshot(snapshot) {
  lineEvEl.textContent = snapshot.line_ev ?? '--';
  busesFoundEl.textContent = snapshot.buses_found ?? 0;
  bestEtaEl.textContent = formatEta(snapshot.best_prediction?.eta_min);
  timestampEl.textContent = snapshot.timestamp ?? '--';
  stopLatEl.textContent = snapshot.stop?.lat ?? '--';
  stopLonEl.textContent = snapshot.stop?.lon ?? '--';
  bestVehicleEl.textContent = snapshot.best_prediction?.vehicle_id ?? '--';
  receivedAtEl.textContent = snapshot.received_at ?? '--';

  summaryTextEl.textContent = snapshot.buses_found > 0
    ? `A linha/EV ${snapshot.line_ev} possui ${snapshot.buses_found} veículo(s) detectado(s) no último ciclo. A melhor previsão atual é do veículo ${snapshot.best_prediction?.vehicle_id ?? 'N/D'}, com ETA estimado em ${formatEta(snapshot.best_prediction?.eta_min)}.`
    : 'Nenhum veículo foi encontrado para a linha monitorada no último ciclo do sensor virtual.';

  renderTable(snapshot.nearest_buses || []);
  statusEl.textContent = 'Dados sincronizados';
}

async function loadStatus() {
  statusEl.textContent = 'Atualizando...';
  try {
    const response = await fetch('/api/status', { cache: 'no-store' });
    if (!response.ok) throw new Error('Falha ao consultar /api/status');
    const snapshot = await response.json();
    renderSnapshot(snapshot);
  } catch (error) {
    console.error(error);
    statusEl.textContent = 'Falha de comunicação';
    summaryTextEl.textContent = 'Não foi possível consultar o servidor do dashboard neste momento.';
  }
}

refreshBtn.addEventListener('click', loadStatus);
loadStatus();
setInterval(loadStatus, 10000);
