# BH-BUS

## Descricao do projeto

O BH-BUS e um sistema academico de monitoramento de onibus em tempo real. A
aplicacao consulta os dados publicos de transporte de Belo Horizonte, localiza
os veiculos de uma linha, calcula a distancia ate uma parada configurada e
estima o tempo de chegada com base na velocidade atual.

O sistema e composto por tres partes:

- um sensor virtual em C, responsavel pela coleta e pelo processamento;
- uma API HTTP em Node.js, responsavel por receber e disponibilizar os dados;
- um dashboard web, responsavel por apresentar as informacoes ao usuario.

O objetivo e simular uma solucao de monitoramento de parada que exiba os
onibus mais proximos, suas distancias, velocidades, movimentos e previsoes de
chegada.

## Tecnologias escolhidas

| Tecnologia | Uso no projeto |
| --- | --- |
| C 99 | Sensor virtual, processamento dos dados e calculo das previsoes |
| CMake | Configuracao e compilacao do sensor |
| libcurl | Requisicoes HTTP feitas pelo sensor |
| cJSON | Leitura e geracao de dados JSON em C |
| Node.js 20+ | Execucao do servidor HTTP |
| Express | API REST e hospedagem do dashboard |
| React | Interface web do dashboard |
| TypeScript | Tipagem e desenvolvimento do frontend |
| Vite | Build e ambiente de desenvolvimento do dashboard |
| Bash | Inicializacao integrada dos componentes |
| WSL/Ubuntu | Ambiente Linux recomendado no Windows |

## Estrutura de pastas

```text
bhbus/
|-- configs/
|   `-- stops.conf              # Configuracao das paradas monitoradas
|-- dashboard/
|   |-- src/
|   |   |-- components/         # Componentes visuais do React
|   |   |-- hooks/              # Atualizacao periodica dos dados
|   |   |-- services/           # Comunicacao com a API
|   |   |-- types/              # Tipos TypeScript
|   |   `-- utils/              # Funcoes auxiliares
|   |-- package.json
|   `-- vite.config.ts
|-- include/                    # Cabecalhos do sensor em C
|-- server/
|   |-- package.json
|   `-- server.js               # Servidor Express e rotas da API
|-- src/                        # Codigo-fonte do sensor em C
|-- bhbus                       # Inicializador integrado
|-- CMakeLists.txt              # Configuracao de build do sensor
`-- README.md
```

## Como rodar

### Pre-requisitos

O ambiente recomendado e Ubuntu 22.04 no WSL 2. Antes de executar o projeto,
instale:

- CMake;
- compilador GCC;
- libcurl e cJSON para desenvolvimento;
- Node.js 20 ou superior;
- npm.

No Ubuntu/WSL, as dependencias nativas podem ser instaladas com:

```bash
sudo apt update
sudo apt install -y build-essential cmake libcurl4-openssl-dev libcjson-dev
```

O dashboard requer Node.js `20.19+` ou `22.12+`.

### Execucao integrada

Na raiz do projeto, execute:

```bash
./bhbus
```

O inicializador instala as dependencias npm quando necessario, compila o
dashboard e o sensor, inicia o servidor e comeca o monitoramento da parada.

Depois, acesse:

```text
http://localhost:3000
```

Use `Ctrl+C` para encerrar o sensor e o servidor.

## Configuracao da parada

As coordenadas e o codigo da linha ficam em `configs/stops.conf`, evitando que
o usuario precise informa-los na linha de comando. O codigo da linha e o campo
`NL` do feed de tempo real; o campo `EV` do feed representa o evento de posicao
do veiculo, normalmente `105`.

```text
# nome;codigo_linha_nl;latitude;longitude[;sentido_sv]
central;105;-19.923450;-43.945670
```

O campo `sentido_sv` e opcional. Use `1`, `2` ou outro valor informado pelo
feed quando quiser monitorar apenas um sentido da viagem; omita o campo para
considerar todos os sentidos.

A primeira linha ativa e selecionada por padrao. Caso existam varias paradas,
uma delas pode ser escolhida pelo nome:

```bash
BHBUS_STOP=central ./bhbus
```

## Execucao manual para desenvolvimento

Os componentes tambem podem ser executados separadamente.

Servidor:

```bash
cd server
npm install
npm start
```

Dashboard:

```bash
cd dashboard
npm install
npm run dev
```

Sensor:

```bash
cmake -S . -B ~/.cache/bhbus/build
cmake --build ~/.cache/bhbus/build -j 4
export BHBUS_API_URL=http://localhost:3000/api/snapshots
~/.cache/bhbus/build/bus_stop_monitor 105 -19.923450 -43.945670
# opcionalmente, filtrando pelo sentido SV=2:
~/.cache/bhbus/build/bus_stop_monitor 105 -19.923450 -43.945670 2
```

O primeiro argumento do sensor e o codigo `NL` da linha monitorada. O sensor
filtra apenas registros de localizacao (`EV=105`) dessa linha, mantem a leitura
mais recente de cada veiculo (`NV`) e envia ao servidor o total de veiculos
distintos encontrados. Quando o quarto argumento opcional `SENTIDO_SV` e
informado, o sensor tambem filtra pelo sentido da viagem.

## Rotas da API

| Metodo | Rota | Descricao |
| --- | --- | --- |
| `GET` | `/health` | Verifica se o servidor esta ativo |
| `GET` | `/api/status` | Retorna o snapshot mais recente |
| `POST` | `/api/snapshots` | Recebe um snapshot enviado pelo sensor |
