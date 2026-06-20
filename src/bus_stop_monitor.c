#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <locale.h>

#include <curl/curl.h>
#include <cJSON.h>

#include "http_client.h"
#include "eta_predictor.h"

#define JSON_URL "https://temporeal.pbh.gov.br/?param=D"
#define SLEEP_SECONDS 20
#define MAX_TOP_BUSES 3
#define DEFAULT_API_URL "http://localhost:3000/api/snapshots"
#define POSITION_EVENT_CODE 105
#define TREND_WINDOW_CYCLES 5
#define HISTORY_MISSING_TOLERANCE_CYCLES 3

typedef struct {
    char vehicle_id[32];
    int line_code;
    double lat;
    double lon;
    double speed_kmh;
    int direction_deg;
    int trip_direction;
    char timestamp_raw[32];
    double distance_m;
} BusReading;

typedef struct {
    char vehicle_id[32];
    double distances_m[TREND_WINDOW_CYCLES];
    size_t distance_count;
    int missing_cycles;
} HistoryItem;

typedef struct {
    HistoryItem *items;
    size_t count;
} HistoryStore;

typedef struct {
    int target_line_code;
    int target_trip_direction;
    double stop_lat;
    double stop_lon;
    char latest_timestamp[32];
    BusReading *buses;
    size_t bus_count;
} SnapshotResult;

static void print_usage(const char *program_name) {
    printf("Uso:\n");
    printf("  %s <CODIGO_LINHA_NL> <LAT_PARADA> <LON_PARADA> [SENTIDO_SV]\n", program_name);
    printf("\nExemplo:\n");
    printf("  %s 105 -19.923450 -43.945670\n", program_name);
}

static void format_timestamp(const char *raw, char *formatted, size_t size) {
    if (!raw || strlen(raw) < 14) {
        snprintf(formatted, size, "desconhecido");
        return;
    }

    snprintf(formatted, size,
             "%c%c%c%c-%c%c-%c%c %c%c:%c%c:%c%c",
             raw[0], raw[1], raw[2], raw[3],
             raw[4], raw[5], raw[6], raw[7],
             raw[8], raw[9], raw[10], raw[11],
             raw[12], raw[13]);
}

static int compare_bus_distance(const void *a, const void *b) {
    const BusReading *left = (const BusReading *)a;
    const BusReading *right = (const BusReading *)b;

    if (left->distance_m < right->distance_m) return -1;
    if (left->distance_m > right->distance_m) return 1;
    return 0;
}

static int find_bus_by_vehicle_id(const BusReading *buses, size_t count, const char *vehicle_id) {
    if (!buses || !vehicle_id) {
        return -1;
    }

    for (size_t i = 0; i < count; i++) {
        if (strcmp(buses[i].vehicle_id, vehicle_id) == 0) {
            return (int)i;
        }
    }

    return -1;
}

static int history_find_in_items(const HistoryItem *items,
                                 size_t count,
                                 const char *vehicle_id,
                                 const HistoryItem **out_item) {
    if (!items || !vehicle_id) {
        return 0;
    }

    for (size_t i = 0; i < count; i++) {
        if (strcmp(items[i].vehicle_id, vehicle_id) == 0) {
            if (out_item) {
                *out_item = &items[i];
            }
            return 1;
        }
    }

    return 0;
}

static int history_find(const HistoryStore *history,
                        const char *vehicle_id,
                        const HistoryItem **out_item) {
    if (!history) {
        return 0;
    }

    return history_find_in_items(history->items, history->count, vehicle_id, out_item);
}

static void history_replace(HistoryStore *history, const BusReading *buses, size_t count) {
    HistoryItem *old_items = history->items;
    size_t old_count = history->count;

    if (count == 0 && old_count == 0) {
        history->items = NULL;
        history->count = 0;
        return;
    }

    HistoryItem *new_items = calloc(count + old_count, sizeof(HistoryItem));
    if (!new_items) {
        free(old_items);
        history->items = NULL;
        history->count = 0;
        return;
    }

    size_t new_count = 0;
    for (size_t i = 0; i < count; i++) {
        const HistoryItem *previous_item = NULL;
        int has_previous = history_find_in_items(old_items, old_count, buses[i].vehicle_id, &previous_item);
        HistoryItem *new_item = &new_items[new_count++];

        snprintf(new_item->vehicle_id, sizeof(new_item->vehicle_id), "%s", buses[i].vehicle_id);

        if (has_previous && previous_item && previous_item->distance_count > 0) {
            size_t copy_count = previous_item->distance_count;
            if (copy_count >= TREND_WINDOW_CYCLES) {
                copy_count = TREND_WINDOW_CYCLES - 1;
                memcpy(new_item->distances_m,
                       previous_item->distances_m + 1,
                       copy_count * sizeof(double));
            } else {
                memcpy(new_item->distances_m,
                       previous_item->distances_m,
                       copy_count * sizeof(double));
            }
            new_item->distances_m[copy_count] = buses[i].distance_m;
            new_item->distance_count = copy_count + 1;
        } else {
            new_item->distances_m[0] = buses[i].distance_m;
            new_item->distance_count = 1;
        }

        new_item->missing_cycles = 0;
    }

    for (size_t i = 0; i < old_count; i++) {
        if (find_bus_by_vehicle_id(buses, count, old_items[i].vehicle_id) >= 0) {
            continue;
        }

        if (old_items[i].missing_cycles + 1 > HISTORY_MISSING_TOLERANCE_CYCLES) {
            continue;
        }

        new_items[new_count] = old_items[i];
        new_items[new_count].missing_cycles++;
        new_count++;
    }

    free(old_items);
    history->items = new_items;
    history->count = new_count;
}

static size_t extract_matching_buses(const char *raw_json,
                                     int target_line_code,
                                     int target_trip_direction,
                                     double stop_lat,
                                     double stop_lon,
                                     BusReading **out_buses,
                                     char *latest_raw_hr,
                                     size_t latest_size) {
    *out_buses = NULL;
    if (latest_raw_hr && latest_size > 0) {
        latest_raw_hr[0] = '\0';
    }

    cJSON *root = cJSON_Parse(raw_json);
    if (!root || !cJSON_IsArray(root)) {
        cJSON_Delete(root);
        return 0;
    }

    int array_size = cJSON_GetArraySize(root);
    if (array_size <= 0) {
        cJSON_Delete(root);
        return 0;
    }

    BusReading *buses = calloc((size_t)array_size, sizeof(BusReading));
    if (!buses) {
        cJSON_Delete(root);
        return 0;
    }

    size_t count = 0;
    cJSON *item = NULL;
    cJSON_ArrayForEach(item, root) {
        cJSON *ev = cJSON_GetObjectItemCaseSensitive(item, "EV");
        cJSON *hr = cJSON_GetObjectItemCaseSensitive(item, "HR");
        cJSON *lt = cJSON_GetObjectItemCaseSensitive(item, "LT");
        cJSON *lg = cJSON_GetObjectItemCaseSensitive(item, "LG");
        cJSON *nv = cJSON_GetObjectItemCaseSensitive(item, "NV");
        cJSON *vl = cJSON_GetObjectItemCaseSensitive(item, "VL");
        cJSON *dg = cJSON_GetObjectItemCaseSensitive(item, "DG");
        cJSON *nl = cJSON_GetObjectItemCaseSensitive(item, "NL");
        cJSON *sv = cJSON_GetObjectItemCaseSensitive(item, "SV");

        if (!cJSON_IsString(ev) || !cJSON_IsString(lt) || !cJSON_IsString(lg) ||
            !cJSON_IsString(nv) || !cJSON_IsString(nl)) {
            continue;
        }

        if (atoi(ev->valuestring) != POSITION_EVENT_CODE || atoi(nl->valuestring) != target_line_code) {
            continue;
        }

        if (target_trip_direction > 0 &&
            (!cJSON_IsString(sv) || atoi(sv->valuestring) != target_trip_direction)) {
            continue;
        }

        int existing_index = find_bus_by_vehicle_id(buses, count, nv->valuestring);
        if (existing_index >= 0) {
            BusReading *existing_bus = &buses[existing_index];
            const char *existing_hr = existing_bus->timestamp_raw;
            const char *current_hr = cJSON_IsString(hr) ? hr->valuestring : "";

            if (strcmp(current_hr, existing_hr) <= 0) {
                continue;
            }

            BusReading updated_bus = {0};
            snprintf(updated_bus.vehicle_id, sizeof(updated_bus.vehicle_id), "%s", nv->valuestring);
            updated_bus.line_code = atoi(nl->valuestring);
            updated_bus.lat = atof(lt->valuestring);
            updated_bus.lon = atof(lg->valuestring);
            updated_bus.speed_kmh = cJSON_IsString(vl) ? atof(vl->valuestring) : 0.0;
            updated_bus.direction_deg = cJSON_IsString(dg) ? atoi(dg->valuestring) : 0;
            updated_bus.trip_direction = cJSON_IsString(sv) ? atoi(sv->valuestring) : 0;
            if (cJSON_IsString(hr)) {
                snprintf(updated_bus.timestamp_raw, sizeof(updated_bus.timestamp_raw), "%s", hr->valuestring);
            }
            updated_bus.distance_m = haversine_m(stop_lat, stop_lon, updated_bus.lat, updated_bus.lon);
            *existing_bus = updated_bus;
        } else {
            BusReading *bus = &buses[count++];
            snprintf(bus->vehicle_id, sizeof(bus->vehicle_id), "%s", nv->valuestring);
            bus->line_code = atoi(nl->valuestring);
            bus->lat = atof(lt->valuestring);
            bus->lon = atof(lg->valuestring);
            bus->speed_kmh = cJSON_IsString(vl) ? atof(vl->valuestring) : 0.0;
            bus->direction_deg = cJSON_IsString(dg) ? atoi(dg->valuestring) : 0;
            bus->trip_direction = cJSON_IsString(sv) ? atoi(sv->valuestring) : 0;

            if (cJSON_IsString(hr)) {
                snprintf(bus->timestamp_raw, sizeof(bus->timestamp_raw), "%s", hr->valuestring);
            }

            bus->distance_m = haversine_m(stop_lat, stop_lon, bus->lat, bus->lon);
        }

        if (cJSON_IsString(hr) && latest_raw_hr && latest_size > 0 && strcmp(hr->valuestring, latest_raw_hr) > 0) {
            snprintf(latest_raw_hr, latest_size, "%s", hr->valuestring);
        }
    }

    cJSON_Delete(root);

    if (count == 0) {
        free(buses);
        return 0;
    }

    qsort(buses, count, sizeof(BusReading), compare_bus_distance);
    *out_buses = buses;
    return count;
}

static double prediction_eta_minutes(const BusReading *bus,
                                     const HistoryStore *history,
                                     const char **movement_out) {
    const HistoryItem *history_item = NULL;
    int has_history = history_find(history, bus->vehicle_id, &history_item);
    const char *movement = "sem historico";

    if (has_history && history_item && history_item->distance_count >= TREND_WINDOW_CYCLES - 1) {
        size_t oldest_index = history_item->distance_count - (TREND_WINDOW_CYCLES - 1);
        double oldest_distance = history_item->distances_m[oldest_index];
        movement = bus->distance_m <= oldest_distance ? "aproximando" : "afastando";
    }

    if (movement_out) {
        *movement_out = movement;
    }
    return estimate_eta_minutes(bus->distance_m, bus->speed_kmh);
}

static int find_best_prediction(const SnapshotResult *snapshot,
                                const HistoryStore *history,
                                double *best_eta) {
    int best_index = -1;
    double current_best_eta = 0.0;

    for (size_t i = 0; i < snapshot->bus_count; i++) {
        const char *movement = NULL;
        double eta_min = prediction_eta_minutes(&snapshot->buses[i], history, &movement);

        if (eta_min < 0.0 || strcmp(movement, "aproximando") != 0) {
            continue;
        }

        if (best_index < 0 || eta_min < current_best_eta) {
            best_index = (int)i;
            current_best_eta = eta_min;
        }
    }

    if (best_eta) {
        *best_eta = current_best_eta;
    }

    return best_index;
}

static char *build_snapshot_json(const SnapshotResult *snapshot, const HistoryStore *history) {
    cJSON *root = cJSON_CreateObject();
    cJSON *stop = cJSON_CreateObject();
    cJSON *nearest = cJSON_CreateArray();
    if (!root || !stop || !nearest) {
        cJSON_Delete(root);
        cJSON_Delete(stop);
        cJSON_Delete(nearest);
        return NULL;
    }

    cJSON_AddNumberToObject(root, "line_code", snapshot->target_line_code);
    cJSON_AddNumberToObject(root, "line_ev", snapshot->target_line_code);
    if (snapshot->target_trip_direction > 0) {
        cJSON_AddNumberToObject(root, "trip_direction_filter", snapshot->target_trip_direction);
    } else {
        cJSON_AddNullToObject(root, "trip_direction_filter");
    }
    cJSON_AddStringToObject(root, "line_label", "N/D");
    cJSON_AddItemToObject(root, "stop", stop);
    cJSON_AddNumberToObject(stop, "lat", snapshot->stop_lat);
    cJSON_AddNumberToObject(stop, "lon", snapshot->stop_lon);

    char formatted_timestamp[32];
    format_timestamp(snapshot->latest_timestamp, formatted_timestamp, sizeof(formatted_timestamp));
    cJSON_AddStringToObject(root, "timestamp", formatted_timestamp);
    cJSON_AddNumberToObject(root, "buses_found", (double)snapshot->bus_count);

    double best_eta = 0.0;
    int best_index = find_best_prediction(snapshot, history, &best_eta);
    size_t added = 0;

    for (size_t i = 0; i < snapshot->bus_count && added < MAX_TOP_BUSES; i++) {
        const char *movement = NULL;
        double eta_min = prediction_eta_minutes(&snapshot->buses[i], history, &movement);

        cJSON *bus = cJSON_CreateObject();
        if (!bus) {
            cJSON_Delete(root);
            return NULL;
        }

        cJSON_AddStringToObject(bus, "vehicle_id", snapshot->buses[i].vehicle_id);
        cJSON_AddNumberToObject(bus, "distance_m", snapshot->buses[i].distance_m);
        cJSON_AddNumberToObject(bus, "speed_kmh", snapshot->buses[i].speed_kmh);
        cJSON_AddNumberToObject(bus, "direction_deg", snapshot->buses[i].direction_deg);
        cJSON_AddNumberToObject(bus, "trip_direction", snapshot->buses[i].trip_direction);
        if (eta_min >= 0.0) {
            cJSON_AddNumberToObject(bus, "eta_min", eta_min);
        } else {
            cJSON_AddNullToObject(bus, "eta_min");
        }
        cJSON_AddStringToObject(bus, "movement", movement);
        cJSON_AddItemToArray(nearest, bus);
        added++;
    }

    cJSON_AddItemToObject(root, "nearest_buses", nearest);

    cJSON *prediction = cJSON_CreateObject();
    if (!prediction) {
        cJSON_Delete(root);
        return NULL;
    }
    if (best_index >= 0) {
        cJSON_AddStringToObject(prediction, "vehicle_id", snapshot->buses[best_index].vehicle_id);
        cJSON_AddNumberToObject(prediction, "eta_min", best_eta);
    } else {
        cJSON_AddStringToObject(prediction, "vehicle_id", "N/D");
        cJSON_AddNullToObject(prediction, "eta_min");
    }
    cJSON_AddItemToObject(root, "best_prediction", prediction);

    char *serialized = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return serialized;
}

static void print_console_snapshot(const SnapshotResult *snapshot, const HistoryStore *history) {
    char latest_formatted[32];
    format_timestamp(snapshot->latest_timestamp, latest_formatted, sizeof(latest_formatted));

    printf("--- PREVISAO DE PARADA (BH-BUS) ----------------\n");
    printf("Linha/NL:          %d\n", snapshot->target_line_code);
    printf("Evento posicao:    %d\n", POSITION_EVENT_CODE);
    if (snapshot->target_trip_direction > 0) {
        printf("Sentido/SV:        %d\n", snapshot->target_trip_direction);
    } else {
        printf("Sentido/SV:        todos\n");
    }
    printf("Descricao:         N/D\n");
    printf("Parada alvo:       %.6f, %.6f\n", snapshot->stop_lat, snapshot->stop_lon);
    printf("Ultimo dado lido:  %s\n", latest_formatted);
    printf("Onibus no feed:    %zu\n\n", snapshot->bus_count);

    if (snapshot->bus_count == 0) {
        printf("Observacao: nenhum veiculo dessa linha apareceu no ciclo atual.\n");
        printf("------------------------------------------------\n");
        return;
    }

    printf("Mais proximos da parada:\n");
    double best_eta = 0.0;
    int best_index = find_best_prediction(snapshot, history, &best_eta);
    size_t printed = 0;

    for (size_t i = 0; i < snapshot->bus_count && printed < MAX_TOP_BUSES; i++) {
        const char *movement = NULL;
        double eta_min = prediction_eta_minutes(&snapshot->buses[i], history, &movement);

        printf("%zu) Veiculo NV=%s | dist=%.0f m | vel=%.0f km/h | dir=%d | ",
               printed + 1,
               snapshot->buses[i].vehicle_id,
               snapshot->buses[i].distance_m,
               snapshot->buses[i].speed_kmh,
               snapshot->buses[i].direction_deg);

        if (eta_min >= 0.0) {
            printf("ETA=%.1f min | ", eta_min);
        } else {
            printf("ETA=indef. | ");
        }
        printf("%s\n", movement);
        printed++;
    }

    printf("\n");
    if (best_index >= 0) {
        printf("Estimativa principal: veiculo %s em %.1f min (distancia atual %.0f m).\n",
               snapshot->buses[best_index].vehicle_id,
               best_eta,
               snapshot->buses[best_index].distance_m);
    } else {
        printf("Estimativa principal: sem velocidade suficiente para prever chegada neste ciclo.\n");
    }

    printf("Observacao: estimativa por distancia em linha reta + velocidade instantanea.\n");
    printf("------------------------------------------------\n");
}

int main(int argc, char *argv[]) {
    setlocale(LC_ALL, "");

    if (argc != 4 && argc != 5) {
        print_usage(argv[0]);
        return 1;
    }

    const int target_line_code = atoi(argv[1]);
    const int target_trip_direction = argc == 5 ? atoi(argv[4]) : 0;
    const double stop_lat = atof(argv[2]);
    const double stop_lon = atof(argv[3]);
    const char *api_url = getenv("BHBUS_API_URL");

    if (target_line_code <= 0) {
        fprintf(stderr, "Codigo da linha/NL invalido: %s\n", argv[1]);
        return 1;
    }
    if (target_trip_direction < 0) {
        fprintf(stderr, "Sentido/SV invalido: %s\n", argv[4]);
        return 1;
    }
    if (!api_url || api_url[0] == '\0') {
        api_url = DEFAULT_API_URL;
    }

    curl_global_init(CURL_GLOBAL_ALL);

    HistoryStore history = {0};

    printf("MONITOR DE PREVISAO DE PARADA - BH-BUS\n");
    printf("Linha/NL informada:   %d\n", target_line_code);
    printf("Evento de posicao:    %d\n", POSITION_EVENT_CODE);
    if (target_trip_direction > 0) {
        printf("Sentido/SV filtrado:  %d\n", target_trip_direction);
    } else {
        printf("Sentido/SV filtrado:  todos\n");
    }
    printf("Descricao:            N/D\n");
    printf("Parada monitorada:    %.6f, %.6f\n", stop_lat, stop_lon);
    printf("Intervalo de leitura: %d segundos\n", SLEEP_SECONDS);
    printf("API destino:          %s\n", api_url);

    while (1) {
        time_t now = time(NULL);
        struct tm *tm_now = localtime(&now);

        printf("\n[%02d:%02d:%02d] >> Acordando dispositivo...\n",
               tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec);

        char *payload = fetch_url(JSON_URL, 20L);
        if (!payload) {
            printf(">> Erro ao baixar o JSON de tempo real.\n");
            printf(">> Ciclo finalizado. Entrando em modo sleep por %d segundos.\n", SLEEP_SECONDS);
            sleep(SLEEP_SECONDS);
            continue;
        }

        SnapshotResult snapshot = {0};
        snapshot.target_line_code = target_line_code;
        snapshot.target_trip_direction = target_trip_direction;
        snapshot.stop_lat = stop_lat;
        snapshot.stop_lon = stop_lon;
        snapshot.bus_count = extract_matching_buses(payload,
                                                    target_line_code,
                                                    target_trip_direction,
                                                    stop_lat,
                                                    stop_lon,
                                                    &snapshot.buses,
                                                    snapshot.latest_timestamp,
                                                    sizeof(snapshot.latest_timestamp));
        free(payload);

        print_console_snapshot(&snapshot, &history);

        char *snapshot_json = build_snapshot_json(&snapshot, &history);
        if (snapshot_json) {
            if (post_json(api_url, snapshot_json, 10L)) {
                printf(">> Snapshot enviado ao servidor.\n");
            } else {
                printf(">> Falha ao enviar snapshot ao servidor.\n");
            }
            free(snapshot_json);
        }

        history_replace(&history, snapshot.buses, snapshot.bus_count);
        free(snapshot.buses);

        printf(">> Ciclo finalizado. Entrando em modo sleep por %d segundos.\n", SLEEP_SECONDS);
        sleep(SLEEP_SECONDS);
    }

    free(history.items);
    curl_global_cleanup();
    return 0;
}
