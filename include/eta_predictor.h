#ifndef ETA_PREDICTOR_H
#define ETA_PREDICTOR_H

double haversine_m(double lat1, double lon1, double lat2, double lon2);
double estimate_eta_minutes(double distance_m, double speed_kmh);
const char *movement_label(double previous_distance_m, double current_distance_m, int has_previous);

#endif
