#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <unistd.h>
#include <time.h>
#include <cjson/cJSON.h>
#include <curl/curl.h>

#include "http_client.h"
#include "eta_predictor.h"

#define JSON_URL "https://temporeal.pbh.gov.br/?param=D"
#define CSV_URL "https://ckan.pbh.gov.br/dataset/730aaa4b-d14c-4755-aed6-433cb0ad9430/resource/150bddd0-9a2c-4731-ade9-54aa56717fb6/download/bhtrans_bdlinha.csv"

#define SLEEP_SECONDS 20
#define MAX_TOP_BUSES 3

typedef struct
{
    char vehicle_id[32];
    double lat;
    double lon;
    double speed_kmh;
    int direction_deg;
    char timestamp_raw[32];
    double distance_m;
} BusReading;

typedef struct
{
    char vehicle_id[32];
    double distance_m;
} HistoryItem;

typedef struct
{
    HistoryItem *items;
    size_t count;
} HistoryStore;

static void print_usage(const char *program_name)
{
    printf("Uso:\n");
    printf("  %s <LINHA_REAL> <LAT_PARADA> <LON_PARADA>\n", program_name);
    printf("\nExemplo:\n");
    printf("  %s 105 -19.923450 -43.945670\n", program_name);
}

static void format_timestamp(const char *raw, char *formatted, size_t size)
{
    if (!raw || strlen(raw) < 14)
    {
        snprintf(formatted, size, "desconhecido");
        return;
    }

    snprintf(
        formatted,
        size,
        "%c%c%c%c-%c%c-%c%c %c%c:%c%c:%c%c",
        raw[0], raw[1], raw[2], raw[3],
        raw[4], raw[5],
        raw[6], raw[7],
        raw[8], raw[9],
        raw[10], raw[11],
        raw[12], raw[13]);
}

static int compare_bus_distance(const void *a, const void *b)
{
    const BusReading *left = (const BusReading *)a;
    const BusReading *right = (const BusReading *)b;

    if (left->distance_m < right->distance_m)
        return -1;
    if (left->distance_m > right->distance_m)
        return 1;
    return 0;
}

static int history_find(const HistoryStore *history, const char *vehicle_id, double *distance_m)
{
    if (!history || !vehicle_id)
    {
        return 0;
    }

    for (size_t i = 0; i < history->count; i++)
    {
        if (strcmp(history->items[i].vehicle_id, vehicle_id) == 0)
        {
            if (distance_m)
            {
                *distance_m = history->items[i].distance_m;
            }
            return 1;
        }
    }

    return 0;
}

static void history_replace(HistoryStore *history, const BusReading *buses, size_t count)
{
    free(history->items);
    history->items = NULL;
    history->count = 0;

    if (!buses || count == 0)
    {
        return;
    }

    history->items = calloc(count, sizeof(HistoryItem));
    if (!history->items)
    {
        return;
    }

    history->count = count;

    for (size_t i = 0; i < count; i++)
    {
        snprintf(history->items[i].vehicle_id, sizeof(history->items[i].vehicle_id), "%s", buses[i].vehicle_id);
        history->items[i].distance_m = buses[i].distance_m;
    }
}

static size_t extract_matching_buses(
    const char *raw_json,
    int target_ev_code,
    double stop_lat,
    double stop_lon,
    BusReading **out_buses,
    char *latest_raw_hr,
    size_t latest_size)
{
    *out_buses = NULL;
    if (latest_raw_hr && latest_size > 0)
    {
        latest_raw_hr[0] = '\0';
    }

    cJSON *root = cJSON_Parse(raw_json);
    if (!root || !cJSON_IsArray(root))
    {
        cJSON_Delete(root);
        return 0;
    }

    int array_size = cJSON_GetArraySize(root);
    if (array_size <= 0)
    {
        cJSON_Delete(root);
        return 0;
    }

    BusReading *buses = calloc((size_t)array_size, sizeof(BusReading));
    if (!buses)
    {
        cJSON_Delete(root);
        return 0;
    }

    size_t count = 0;

    cJSON *item = NULL;
    cJSON_ArrayForEach(item, root)
    {
        cJSON *ev = cJSON_GetObjectItemCaseSensitive(item, "EV");
        cJSON *hr = cJSON_GetObjectItemCaseSensitive(item, "HR");
        cJSON *lt = cJSON_GetObjectItemCaseSensitive(item, "LT");
        cJSON *lg = cJSON_GetObjectItemCaseSensitive(item, "LG");
        cJSON *nv = cJSON_GetObjectItemCaseSensitive(item, "NV");
        cJSON *vl = cJSON_GetObjectItemCaseSensitive(item, "VL");
        cJSON *dg = cJSON_GetObjectItemCaseSensitive(item, "DG");

        if (!cJSON_IsString(ev) || !cJSON_IsString(lt) || !cJSON_IsString(lg) || !cJSON_IsString(nv))
        {
            continue;
        }

        if (atoi(ev->valuestring) != target_ev_code)
        {
            continue;
        }

        BusReading *bus = &buses[count++];
        snprintf(bus->vehicle_id, sizeof(bus->vehicle_id), "%s", nv->valuestring);
        bus->lat = atof(lt->valuestring);
        bus->lon = atof(lg->valuestring);
        bus->speed_kmh = cJSON_IsString(vl) ? atof(vl->valuestring) : 0.0;
        bus->direction_deg = cJSON_IsString(dg) ? atoi(dg->valuestring) : 0;

        if (cJSON_IsString(hr))
        {
            snprintf(bus->timestamp_raw, sizeof(bus->timestamp_raw), "%s", hr->valuestring);

            if (latest_raw_hr && latest_size > 0 && strcmp(hr->valuestring, latest_raw_hr) > 0)
            {
                snprintf(latest_raw_hr, latest_size, "%s", hr->valuestring);
            }
        }
        else
        {
            bus->timestamp_raw[0] = '\0';
        }

        bus->distance_m = haversine_m(stop_lat, stop_lon, bus->lat, bus->lon);
    }

    cJSON_Delete(root);

    if (count == 0)
    {
        free(buses);
        return 0;
    }

    qsort(buses, count, sizeof(BusReading), compare_bus_distance);
    *out_buses = buses;
    return count;
}

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");

    if (argc != 4)
    {
        print_usage(argv[0]);
        return 1;
    }

    const char *line_input = argv[1];
    const int target_ev_code = atoi(line_input);
    const double stop_lat = atof(argv[2]);
    const double stop_lon = atof(argv[3]);

    curl_global_init(CURL_GLOBAL_ALL);

    if (target_ev_code <= 0)
    {
        fprintf(stderr, "Linha/EV invalido: %s\n", line_input);
        curl_global_cleanup();
        return 1;
    }

    HistoryStore history = {0};

    printf("MONITOR DE PREVISAO DE PARADA - BH-BUS\n");
    printf("Linha/EV informado:   %s\n", line_input);
    printf("Codigo EV usado:      %d\n", target_ev_code);
    printf("Descricao:            N/D\n");
    printf("Parada monitorada:    %.6f, %.6f\n", stop_lat, stop_lon);
    printf("Intervalo de leitura: %d segundos\n", SLEEP_SECONDS);

    while (1)
    {
        time_t now = time(NULL);
        struct tm *tm_now = localtime(&now);

        printf("\n[%02d:%02d:%02d] >> Acordando dispositivo...\n",
               tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec);

        char *payload = fetch_url(JSON_URL, 20L);
        if (!payload)
        {
            printf(">> Erro ao baixar o JSON de tempo real.\n");
            printf(">> Ciclo finalizado. Entrando em modo sleep por %d segundos.\n", SLEEP_SECONDS);
            sleep(SLEEP_SECONDS);
            continue;
        }

        BusReading *buses = NULL;
        char latest_raw_hr[32] = {0};

        size_t bus_count = extract_matching_buses(
            payload,
            target_ev_code,
            stop_lat,
            stop_lon,
            &buses,
            latest_raw_hr,
            sizeof(latest_raw_hr));

        free(payload);

        if (bus_count == 0)
        {
            printf("--- PREVISAO DE PARADA (BH-BUS) ----------------\n");
            printf("Linha/EV:          %s\n", line_input);
            printf("Codigo EV:         %d\n", target_ev_code);
            printf("Descricao:         N/D\n");
            printf("Parada alvo:       %.6f, %.6f\n", stop_lat, stop_lon);
            printf("Onibus no feed:    0\n");
            printf("Observacao: nenhum veiculo dessa linha apareceu no ciclo atual.\n");
            printf("------------------------------------------------\n");
            printf(">> Ciclo finalizado. Entrando em modo sleep por %d segundos.\n", SLEEP_SECONDS);
            sleep(SLEEP_SECONDS);
            continue;
        }

        char latest_formatted[32];
        format_timestamp(latest_raw_hr, latest_formatted, sizeof(latest_formatted));

        printf("--- PREVISAO DE PARADA (BH-BUS) ----------------\n");
        printf("Linha/EV:          %s\n", line_input);
        printf("Codigo EV:         %d\n", target_ev_code);
        printf("Descricao:         N/D\n");
        printf("Parada alvo:       %.6f, %.6f\n", stop_lat, stop_lon);
        printf("Ultimo dado lido:  %s\n", latest_formatted);
        printf("Onibus no feed:    %zu\n\n", bus_count);

        printf("Mais proximos da parada:\n");

        size_t limit = bus_count < MAX_TOP_BUSES ? bus_count : MAX_TOP_BUSES;
        int best_index = -1;
        double best_eta = 0.0;

        for (size_t i = 0; i < limit; i++)
        {
            double previous_distance = 0.0;
            int has_previous = history_find(&history, buses[i].vehicle_id, &previous_distance);
            const char *movement = movement_label(previous_distance, buses[i].distance_m, has_previous);
            double eta_min = estimate_eta_minutes(buses[i].distance_m, buses[i].speed_kmh);

            printf("%zu) Veiculo NV=%s | dist=%.0f m | vel=%.0f km/h | dir=%d | ",
                   i + 1,
                   buses[i].vehicle_id,
                   buses[i].distance_m,
                   buses[i].speed_kmh,
                   buses[i].direction_deg);

            if (eta_min >= 0.0)
            {
                printf("ETA=%.1f min | ", eta_min);
            }
            else
            {
                printf("ETA=indef. | ");
            }

            printf("%s\n", movement);

            if (eta_min >= 0.0 && best_index < 0)
            {
                best_index = (int)i;
                best_eta = eta_min;
            }
        }

        printf("\n");
        if (best_index >= 0)
        {
            printf("Estimativa principal: veiculo %s em %.1f min (distancia atual %.0f m).\n",
                   buses[best_index].vehicle_id,
                   best_eta,
                   buses[best_index].distance_m);
        }
        else
        {
            printf("Estimativa principal: sem velocidade suficiente para prever chegada neste ciclo.\n");
        }

        printf("Observacao: estimativa por distancia em linha reta + velocidade instantanea.\n");
        printf("------------------------------------------------\n");

        history_replace(&history, buses, bus_count);
        free(buses);

        printf(">> Ciclo finalizado. Entrando em modo sleep por %d segundos.\n", SLEEP_SECONDS);
        sleep(SLEEP_SECONDS);
    }

    free(history.items);
    curl_global_cleanup();
    return 0;
}