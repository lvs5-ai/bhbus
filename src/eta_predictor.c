#include <math.h>
#include "eta_predictor.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static double deg2rad(double deg) {
    return deg * M_PI / 180.0;
}

double haversine_m(double lat1, double lon1, double lat2, double lon2) {
    const double earth_radius_m = 6371000.0;

    double dlat = deg2rad(lat2 - lat1);
    double dlon = deg2rad(lon2 - lon1);

    double a = sin(dlat / 2.0) * sin(dlat / 2.0) +
               cos(deg2rad(lat1)) * cos(deg2rad(lat2)) *
               sin(dlon / 2.0) * sin(dlon / 2.0);

    double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
    return earth_radius_m * c;
}

double estimate_eta_minutes(double distance_m, double speed_kmh) {
    if (speed_kmh <= 1.0) {
        return -1.0;
    }

    double meters_per_minute = (speed_kmh * 1000.0) / 60.0;
    return distance_m / meters_per_minute;
}

const char *movement_label(double previous_distance_m, double current_distance_m, int has_previous) {
    if (!has_previous) {
        return "sem historico";
    }

    double delta = current_distance_m - previous_distance_m;

    if (delta < -20.0) {
        return "aproximando";
    }

    if (delta > 20.0) {
        return "afastando";
    }

    return "estavel";
}