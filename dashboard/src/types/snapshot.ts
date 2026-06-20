export interface Bus {
  vehicle_id: string;
  distance_m: number | null;
  speed_kmh: number | null;
  direction_deg: number | null;
  trip_direction?: number | null;
  eta_min: number | null;
  movement: string;
}

export interface Snapshot {
  line_code: number | null;
  line_ev?: number | null;
  line_label: string;
  trip_direction_filter?: number | null;
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
