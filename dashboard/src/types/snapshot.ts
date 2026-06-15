export interface Bus {
  vehicle_id: string;
  distance_m: number | null;
  speed_kmh: number | null;
  direction_deg: number | null;
  eta_min: number | null;
  movement: string;
}

export interface Snapshot {
  line_ev: number | null;
  line_label: string;
  timestamp: string | null;
  received_at?: string;
  buses_found: number;
  stop: {
    lat: number | null;
    lon: number | null;
  };
  nearest_buses: Bus[];
  best_prediction: {
    vehicle_id: string;
    eta_min: number | null;
  };
}
