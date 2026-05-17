const express = require('express');
const path = require('path');
const cors = require('cors');

const app = express();
const PORT = process.env.PORT || 3000;

let latestSnapshot = {
  line_ev: null,
  line_label: 'N/D',
  timestamp: null,
  buses_found: 0,
  stop: { lat: null, lon: null },
  nearest_buses: [],
  best_prediction: { vehicle_id: 'N/D', eta_min: null }
};

app.use(cors());
app.use(express.json());
app.use(express.static(path.join(__dirname, '..', 'dashboard')));

app.get('/health', (_req, res) => {
  res.json({ ok: true, service: 'BH-BUS server' });
});

app.get('/api/status', (_req, res) => {
  res.json(latestSnapshot);
});

app.post('/api/snapshots', (req, res) => {
  latestSnapshot = {
    ...latestSnapshot,
    ...req.body,
    received_at: new Date().toISOString()
  };

  console.log('[snapshot]', {
    line_ev: latestSnapshot.line_ev,
    buses_found: latestSnapshot.buses_found,
    timestamp: latestSnapshot.timestamp
  });

  res.status(201).json({ ok: true });
});

app.get('*', (_req, res) => {
  res.sendFile(path.join(__dirname, '..', 'dashboard', 'index.html'));
});

app.listen(PORT, () => {
  console.log(`BH-BUS server running at http://localhost:${PORT}`);
});
