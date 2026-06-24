# UnitLab RPi Net Agent

Host-side Linux network control service for the UnitLab core (Raspberry Pi 5, Bookworm, `NetworkManager`).

Purpose:
- always boot into AP mode (`[unitlab]-core-ABCD`, `pwd!ABCD`)
- accept Wi-Fi scan / STA connect requests via Redis
- own host RJ45 configuration for MMS/SCADA reachability via Redis commands
- recommend the host interface from the live Linux default route / carrier state when available
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
  - runs `nmcli` for scan / connect / AP restart / interface discovery / Ethernet profile updates
  - stores latest snapshot in Redis (`core_net:state`) including discovered interfaces and the selected host network interface
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
- `apply_network_settings`

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
  "host_network": {
    "interface": "eth0",
    "profile": "unitlab-lan",
    "ipv4_mode": "auto",
    "address_cidr": null,
    "gateway": null,
    "dns_servers": [],
    "proxy_url": null,
    "proxy_no_proxy": [],
    "last_applied_at": null,
    "last_error": null
  },
  "interfaces": [
    {
      "interface_name": "wlan0",
      "local_ip": "10.42.0.1",
      "netmask": "24",
      "network": "10.42.0.0/24",
      "connection": "unitlab-ap",
      "state": "activated"
    },
    {
      "interface_name": "eth0",
      "local_ip": null,
      "netmask": null,
      "network": null,
      "connection": null,
      "state": "disconnected"
    }
  ],
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
- `UNITLAB_NET_AGENT_ETHERNET_IFACE` (default `eth0`)
- `UNITLAB_NET_AGENT_ETHERNET_PROFILE` (default `unitlab-lan`)
- `UNITLAB_NET_AGENT_ETHERNET_DEFAULT_MODE` (default `auto`)
- `UNITLAB_NET_AGENT_ETHERNET_DEFAULT_ADDRESS_CIDR` (default unset)
- `UNITLAB_NET_AGENT_ETHERNET_DEFAULT_GATEWAY` (default unset)
- `UNITLAB_NET_AGENT_ETHERNET_DEFAULT_DNS_SERVERS` (comma-separated, default unset)
- `UNITLAB_NET_AGENT_PROXY_URL` (default unset)
- `UNITLAB_NET_AGENT_PROXY_NO_PROXY` (comma-separated, default unset)
- `UNITLAB_NET_AGENT_HOST_NETWORK_SETTINGS_FILE` (default `/etc/unitlab/rpi-net-agent-network.json`)
- `UNITLAB_NET_AGENT_PROXY_ENV_FILE` (default `/etc/environment.d/50-unitlab-proxy.conf`)
- `UNITLAB_NET_AGENT_STA_CONNECT_TIMEOUT_SEC` (default `35`)
- `UNITLAB_NET_AGENT_LOG_LEVEL` (default `INFO`)
- `UNITLAB_NET_AGENT_DRY_RUN` (`true`/`false`)

## Notes / current limitations

- The agent currently assumes single-radio mode transitions (AP or STA active).
- Host Ethernet settings are persisted through the host agent and applied via `nmcli` on the host OS.
- Interface snapshots expose link and route hints (`carrier`, `oper_state`, `is_default_route`, `default_route_metric`) when Linux host data is available.
- Interface recommendation and link warnings depend on Linux host data from `nmcli`, `/proc/net/route`, and `/sys/class/net`.
- `scan` parsing uses `nmcli -t` output and may need escaping hardening for exotic SSIDs containing separators.
- Backend API/UI integration is expected to talk to Redis using the contract above (separate step).
