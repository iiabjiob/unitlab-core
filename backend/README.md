# UnitLab Backend

FastAPI + AsyncPG + Redis + MQTT backend

## MQTT/Redis Pipeline

The transport layer is now split into standalone workers, so FastAPI stays a thin REST/WS gateway.

### Startup order

Before running any of the commands below in a devcontainer, activate the backend Python environment (e.g. `cd backend && source .venv/bin/activate`) or prefix commands with `uv run` so they reuse the managed virtualenv.

For one-click startup in VS Code, run task `backend: start all` (Terminal → Run Task). It launches all processes below in parallel, each in its own dedicated terminal.

1. FastAPI (REST + WebSockets):
	```bash
	uv run uvicorn app.main:app --host 0.0.0.0 --port 8000 --reload --workers 4
	```
2. MQTT ingress → Redis Stream `mqtt:inbound`:
	```bash
	uv run python -m app.workers.mqtt_ingress
	```
3. Inbound processor (business logic + WS events → Redis Pub/Sub `ws:events`):
	```bash
	uv run python -m app.workers.inbound_processor
	```
4. MQTT outbound publisher ← Redis Stream `mqtt:outbound`:
	```bash
	uv run python -m app.workers.mqtt_outbound
	```
5. Device offline checker (watches `devices:all`, emits WS heartbeats):
	```bash
	uv run python -m app.workers.device_offline
	```
6. **Sequence runner** (consumes `sequence:commands`, emits lifecycle events to `sequence:events`):
	```bash
	uv run python -m app.workers.sequence_runner
	```
7. **Signal allocation runner** (consumes `signal-allocation:jobs`, executes allocation jobs in background):
	```bash
	uv run python -m app.workers.signal_allocation_runner
	```

FastAPI subscribes to `ws:events` and forwards every payload to connected WebSocket clients.

### Sequence execution pipeline

- REST `start/stop` endpoints enqueue control messages into the Redis stream named in `sequence_command_stream` (defaults to `sequence:commands`).
- `app.workers.sequence_runner` is the single consumer (for now) that executes sequences, updates the `sequence_runs` tables, and writes telemetry to `sequence_event_stream` (`sequence:events`).
- FastAPI hosts a background task (`forward_sequence_events`) that tails `sequence:events`, translates each record back into the legacy WS payloads (`started`, `progress`, `completed`, `stopped`, `error`), and publishes them through the existing `ws:events` pub/sub channel via `WsEventPublisher`.
- Frontend clients continue to receive real-time updates with no code changes, while the backend can scale API pods and the runner worker independently.

> In `docker-compose.prod.yml` these workers are defined as separate services (`mqtt_ingress`, `inbound_processor`, `mqtt_outbound`, `device_offline`). To launch the full production stack run `docker compose -f docker-compose.prod.yml up -d`.

## Local docker-compose

For development, `docker-compose.dev.yml` brings up only the shared infrastructure (Postgres, Redis, Mosquitto, devcontainer). FastAPI and all workers should be started manually from the devcontainer using the commands above, which lets you see code changes immediately without rebuilding images.
