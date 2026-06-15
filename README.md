# BH-BUS

Projeto academico de monitoramento de parada usando um sensor virtual em C,
um servidor HTTP em Node.js e um dashboard web.

## Como rodar

No WSL, na raiz do projeto:

```bash
./bhbus
```

O comando prepara as dependencias, compila o dashboard e o sensor, inicia todos
os componentes e disponibiliza o sistema em:

```text
http://localhost:3000
```

Use `Ctrl+C` para encerrar o sensor e o servidor juntos.

## Configurar a parada

A parada monitorada fica em `configs/stops.conf`, sem precisar informar
coordenadas ao iniciar o sistema:

```text
# nome;ev;latitude;longitude
central;105;-19.923450;-43.945670
```

A primeira linha ativa e usada por padrao. Para manter varias paradas no
arquivo e escolher uma sem alterar o codigo:

```bash
BHBUS_STOP=central ./bhbus
```

## Estrutura

- `src/` e `include/`: sensor virtual em C
- `server/`: servidor HTTP e API REST
- `dashboard/`: dashboard React
- `configs/stops.conf`: paradas monitoradas
- `bhbus`: inicializador integrado

## Rotas

- `GET /health`
- `GET /api/status`
- `POST /api/snapshots`
