# UnitLab RPi Net Agent

Host-side Wi-Fi control service for the UnitLab core (Raspberry Pi 5, Bookworm, `NetworkManager`).

Purpose:
- always boot into AP mode (`[unitlab]-core-ABCD`, `pwd!ABCD`)
- accept Wi-Fi scan / STA connect requests via Redis
- publish status/events back to Redis for backend/frontend
- fallback to AP if STA connect fails

This service is intended to run on the **host OS** (not inside Docker containers).

## Behavior (current)

- On startup:
  - reads MAC from `wlan0`
  - generates suffix `ABCD` from last two bytes of MAC
  - builds AP credentials:
    - `SSID = [unitlab]-core-ABCD`
    - `Password = pwd!ABCD`
  - ensures and activates `NetworkManager` AP profile (`unitlab-ap`)
- During runtime:
  - listens for Redis stream commands
  - runs `nmcli` for scan / connect / AP restart
  - stores latest snapshot in Redis (`core_net:state`)
  - writes events to Redis stream (`core_net:events`)

## Requirements (host)

- Raspberry Pi OS Bookworm
- `NetworkManager` + `nmcli`
- Python 3.11+
- Redis reachable from host (typically `127.0.0.1:6379`)

## Install (host)

See:
- `install/install_rpi_net_agent.sh`

Typical:

```bash
sudo /opt/unitlab/install/install_rpi_net_agent.sh
```

## Run manually (dev)

```bash
cd /opt/unitlab/rpi-net-agent
source .venv/bin/activate
unitlab-rpi-net-agent
```

## Redis Contract

### Command stream

- Stream: `core_net:commands`
- Consumer group: `core-net-agent`

Command envelope is stored in a single field `json`:

```json
{
  "request_id": "uuid",
  "action": "connect_sta",
  "ssid": "PlantWiFi",
  "password": "secret",
  "hidden": false,
  "timeout_sec": 35
}
```

Supported actions:
- `status`
- `scan`
- `connect_sta`
- `disconnect_sta`
- `restart_ap`

### Event stream

- Stream: `core_net:events`
- Each entry field: `json=<event-payload-json>`

Events emitted:
- `state`
- `scan_started`
- `scan_result`
- `sta_connecting`
- `sta_connected`
- `sta_connect_failed`
- `ap_active`
- `command_failed`
- `command_rejected`
- `command_invalid`

### State snapshot key

- Key: `core_net:state`

Snapshot example:

```json
{
  "mode": "ap",
  "wifi_iface": "wlan0",
  "mac": "DC:A6:32:12:34:56",
  "suffix": "3456",
  "ap": {
    "ssid": "[unitlab]-core-3456",
    "password": "pwd!3456",
    "profile": "unitlab-ap",
    "iface": "wlan0",
    "ip": "10.42.0.1",
    "active": true
  },
  "sta": {
    "state": "disconnected",
    "ssid": null,
    "profile": null,
    "ip": null,
    "last_error": null
  },
  "request_in_flight": null,
  "last_event": "ap_active",
  "last_error": null,
  "available_networks": [],
  "updated_at": "2026-02-23T00:00:00Z"
}
```

## Environment Variables

- `UNITLAB_NET_AGENT_REDIS_URL` (default `redis://127.0.0.1:6379/0`)
- `UNITLAB_NET_AGENT_WIFI_IFACE` (default `wlan0`)
- `UNITLAB_NET_AGENT_AP_PROFILE` (default `unitlab-ap`)
- `UNITLAB_NET_AGENT_AP_SSID_PREFIX` (default `[unitlab]-core`)
- `UNITLAB_NET_AGENT_AP_PASSWORD_PREFIX` (default `pwd!`)
- `UNITLAB_NET_AGENT_AP_IP_CIDR` (default `10.42.0.1/24`)
- `UNITLAB_NET_AGENT_STA_CONNECT_TIMEOUT_SEC` (default `35`)
- `UNITLAB_NET_AGENT_LOG_LEVEL` (default `INFO`)
- `UNITLAB_NET_AGENT_DRY_RUN` (`true`/`false`)

## Notes / current limitations

- The agent currently assumes single-radio mode transitions (AP or STA active).
- `scan` parsing uses `nmcli -t` output and may need escaping hardening for exotic SSIDs containing separators.
- Backend API/UI integration is expected to talk to Redis using the contract above (separate step).
