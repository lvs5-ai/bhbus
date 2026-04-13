# BH-BUS

Monitor de parada para ônibus de Belo Horizonte, feito em C.

O projeto consome um feed público de tempo real, filtra os veículos de uma linha e estima o tempo de chegada em uma parada informada pelo usuário com base em:

- posição atual do ônibus;
- distância até a parada;
- velocidade instantânea do veículo;
- comparação com ciclos anteriores para indicar se o ônibus está se aproximando ou se afastando.

---

## Objetivo

Capturar as coordenadas dos ônibus para prever o tempo de espera em uma parada.

Atualmente, o programa usa diretamente o valor de `EV` informado na execução para filtrar os veículos no feed JSON.

---

## Fonte dos dados

### JSON de tempo real

Feed público com coordenadas atualizadas dos ônibus em operação:

```text
https://temporeal.pbh.gov.br/?param=D
```

### Observação importante

Nesta versão, o projeto não usa o CSV de conversão de linhas para o filtro principal, pois a relação entre esse arquivo e o campo `EV` não se mostrou confiável para a filtragem adotada.

---

## Estrutura do projeto

```text
BH-BUS/
├─ CMakeLists.txt
├─ build/
├─ src/
│  ├─ bus_stop_monitor.c
│  ├─ eta_predictor.c
│  └─ http_client.c
├─ include/
│  ├─ eta_predictor.h
│  └─ http_client.h
├─ data/
└─ configs/
   └─ stops.conf
```

---

## Dependências

No Linux/WSL, instale:

- `gcc`
- `cmake`
- `make`
- `libcurl`
- `cJSON`

Exemplo para Debian/Ubuntu:

```bash
sudo apt update
sudo apt install build-essential cmake libcurl4-openssl-dev libcjson-dev
```

---

## Compilação

Na raiz do projeto:

```bash
cmake -S . -B build
cmake --build build
```

Executável gerado:

```text
build/bus_stop_monitor
```

---

## Como usar

Formato:

```bash
./build/bus_stop_monitor <EV_DA_LINHA> <LAT_PARADA> <LON_PARADA>
```

### Exemplo 1

```bash
./build/bus_stop_monitor 105 -19.923450 -43.945670
```

### Exemplo 2

```bash
./build/bus_stop_monitor 5106 -19.919000 -43.938000
```

### Exemplo 3

```bash
./build/bus_stop_monitor 8207 -19.885000 -43.950000
```

---

## Exemplo de saída

```text
MONITOR DE PREVISAO DE PARADA - BH-BUS
Linha/EV informado:   105
Codigo EV usado:      105
Descricao:            N/D
Parada monitorada:    -19.923450, -43.945670
Intervalo de leitura: 20 segundos

[22:56:31] >> Acordando dispositivo...
--- PREVISAO DE PARADA (BH-BUS) ----------------
Linha/EV:          105
Codigo EV:         105
Descricao:         N/D
Parada alvo:       -19.923450, -43.945670
Ultimo dado lido:  2026-04-12 22:56:20
Onibus no feed:    3

Mais proximos da parada:
1) Veiculo NV=31277 | dist=420 m | vel=33 km/h | dir=262 | ETA=0.8 min | aproximando
2) Veiculo NV=21021 | dist=710 m | vel=31 km/h | dir=11 | ETA=1.4 min | estavel
3) Veiculo NV=20928 | dist=980 m | vel=10 km/h | dir=173 | ETA=5.9 min | afastando

Estimativa principal: veiculo 31277 em 0.8 min (distancia atual 420 m).
Observacao: estimativa por distancia em linha reta + velocidade instantanea.
------------------------------------------------
>> Ciclo finalizado. Entrando em modo sleep por 20 segundos.
```

---

## Como funciona

O programa executa ciclos contínuos:

1. baixa o JSON de tempo real;
2. filtra apenas os ônibus cujo `EV` seja igual ao valor informado;
3. calcula a distância entre cada veículo e a parada alvo;
4. ordena os ônibus pelos mais próximos;
5. estima o tempo de chegada com base na velocidade instantânea;
6. compara com o ciclo anterior para classificar o movimento como:
   - `aproximando`
   - `afastando`
   - `estavel`
   - `sem historico`
7. entra em sleep por 20 segundos e repete.

---

## Parâmetros de entrada

### `EV_DA_LINHA`

Código da linha exatamente como será usado no filtro do feed.

### `LAT_PARADA`

Latitude da parada que será monitorada.

### `LON_PARADA`

Longitude da parada que será monitorada.

---

## Campos exibidos na saída

- **Linha/EV**: valor informado na execução;
- **Codigo EV**: código usado no filtro;
- **Parada alvo**: coordenadas da parada;
- **Ultimo dado lido**: timestamp mais recente encontrado no ciclo;
- **Onibus no feed**: quantidade de veículos encontrados para aquele `EV`;
- **dist**: distância em metros até a parada;
- **vel**: velocidade instantânea do veículo;
- **dir**: direção reportada no feed;
- **ETA**: tempo estimado até a parada;
- **aproximando / afastando / estavel**: comparação com o ciclo anterior.

---