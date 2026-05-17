# BH-BUS

Projeto acadêmico de monitoramento de parada usando um sensor virtual em C, um servidor HTTP em Node.js e um dashboard web.

## Estrutura

- `src/` e `include/`: sensor virtual em C
- `server/`: servidor HTTP e API REST
- `dashboard/`: tela web do dashboard

## Como rodar

### 1) Sensor virtual

Na raiz do projeto:

```bash
cmake -S . -B build
cmake --build build
```

### 2) Servidor e dashboard

```bash
cd server
npm install
npm start
```

O dashboard ficará disponível em `http://localhost:3000`.

### 3) Enviar snapshots do sensor para o servidor

Em outro terminal, na raiz do projeto:

```bash
export BHBUS_API_URL=http://localhost:3000/api/snapshots
./build/bus_stop_monitor 105 -19.923450 -43.945670
```

## Rotas

- `GET /health`
- `GET /api/status`
- `POST /api/snapshots`
