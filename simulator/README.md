## UnitLab MQTT Device Simulator

This package spins up a fleet of virtual UnitLab devices that speak the same MQTT protocol as the hardware. It is mainly used to:
- Verify backend ingestion (MQTT handlers, state replication, offline checker).
- Stress test WebSocket clients and UI flows without touching real devices.
- Reproduce tricky scenarios such as random disconnects, packet loss, or high message volume.

The simulator is completely CLI-driven (`python -m simulator`) and relies on a single YAML file for defaults. All knobs can be overridden via command-line flags so you can script experiments or CI runs.

---

## Prerequisites

- Python 3.11+ (handled automatically if you run through [`uv`](https://github.com/astral-sh/uv)).
- A reachable MQTT broker (defaults to `mosquitto:1883`).
- Network access from the dev container to that broker.

Recommended invocation uses `uv run`, which isolates dependencies per project:

```bash
uv run python -m simulator
```

`uv` reads `pyproject.toml`, installs pinned dependencies into a shared cache, and executes the module without mutating the repo.

---

## Quick Start

1. **Default run** (uses `config.yaml`, profile `default`):
	```bash
	uv run python -m simulator
	```
2. **Verbose logs + custom profile:**
	```bash
	uv run python -m simulator --profile smoke --verbose
	```
3. **Short-lived test** (stop after N seconds):
	```bash
	uv run python -m simulator --max-runtime 30
	```

The module installs signal handlers; press `Ctrl+C` to stop gracefully. When the simulator exits it sends a stop signal to each virtual device and waits for tasks to finish.

---

## CLI Reference

`python -m simulator --help` prints the authoritative list. Most used flags:

| Flag | Description |
| ---- | ----------- |
| `--config PATH` | Path to YAML config (defaults to `config.yaml` in this folder). |
| `--profile {default,load,smoke,stress}` | Multiplies device counts by the preset factor (0.25–2.0). |
| `--do / --di / --ao` | Override the number of DO/DI/AO devices *after* profiling. |
| `--hex-dump` | Print hex for every packet published. Useful when inspecting protocol bytes. |
| `--chaos` | Forces aggressive reconnects and packet drops to test resiliency. |
| `--max-runtime SECONDS` | Auto-stop after the provided duration. |
| `--verbose` | Switch logger to DEBUG level. |

All arguments are optional; you can mix any combination (e.g., `--profile stress --chaos --max-runtime 120`).

---

## Configuration File (`config.yaml`)

```yaml
broker:
  host: mosquitto
  port: 1883
  keepalive: 30
  username: null
  password: null

devices:
  DO: { count: 5, signals: 32, interval: 2.0 }
  DI: { count: 1, signals: 32, interval: 1.0 }
  AO: { count: 0, signals: 10, interval: 3.0 }

behavior:
  randomize: true
  heartbeat: 5.0
  reconnect_chance: 0.01
  packet_loss: 0.005
	flaky_device_ratio: 0.05
	flaky_exchange_prob: 0.20
	command_error_rate: 0.05
```

### Sections

- **broker** — connection details for the MQTT server. You can override these per environment (e.g., `--config config.dev.yaml`).
- **devices** — three groups (DO, DI, AO). Each group defines how many devices are created, how many signals each device exposes, and the publish interval (seconds).
- **behavior** — controls randomness and network unreliability. Set `packet_loss` to zero for deterministic tests, or enable `--chaos` to multiply drop/reconnect rates on the fly.

### Reliability mode (all packets/commands should pass)

To disable all fault injection paths, set:

```yaml
behavior:
	reconnect_chance: 0.0
	packet_loss: 0.0
	flaky_device_ratio: 0.0
	flaky_exchange_prob: 0.0
	command_error_rate: 0.0
```

- `packet_loss` disables transport drops in the simulator.
- `reconnect_chance` disables random reconnects.
- `flaky_device_ratio` + `flaky_exchange_prob` disable built-in flaky-device request failures.
- `command_error_rate` disables simulated command NACK/ERROR responses.

In addition to the static YAML, the simulator supports runtime scaling:

| Profile | Factor | Notes |
| ------- | ------ | ----- |
| `default` | 1.0 | Whatever is in the YAML. |
| `load` | 1.0 | Alias for `default` (kept for compatibility). |
| `smoke` | 0.25 | Quick low-load verification. |
| `stress` | 2.0 | Doubles every device count. |

Profiles scale *device counts* only. Afterwards you can still override individual counts via `--do`, `--di`, `--ao`.

---

## Typical Workflows

1. **Backend dev loop**
	```bash
	uv run python -m simulator --profile smoke --max-runtime 120
	# Watch backend logs for heartbeat/state packets
	```

2. **Chaos testing**
	```bash
	uv run python -m simulator --profile stress --chaos --hex-dump --verbose
	```

3. **CI smoke test**
	```bash
	uv run python -m simulator --profile smoke --max-runtime 30 --do 1 --di 1
	```

Feel free to version-control extra YAML configs (e.g., `config.staging.yaml`). Nothing in `config.yaml` is secret; credentials should live in separate files or CI secrets.

---

## Troubleshooting

- **Broker unreachable** — check Docker compose: `docker compose ps mosquitto`. Use `broker.host=localhost` when running everything on the same machine.
- **No devices created** — ensure counts remain >0 after profile scaling. (E.g., `smoke` factor may round down small numbers. The code clamps to at least one when the original count was >0.)
- **Simulator never stops** — use `Ctrl+C` or pass `--max-runtime` for automated shutdowns.
- **Need more visibility** — combine `--hex-dump` with `--verbose` to see both decoded values and raw bytes.

That’s it—future you now has the essentials to bring the simulator back online in minutes. 🚀
