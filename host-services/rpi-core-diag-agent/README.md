# UnitLab RPi Core Diagnostics Agent

Host-side diagnostics agent for UnitLab Core (Raspberry Pi 5).

## Purpose

Publishes central-module diagnostics to Redis so backend/frontend can show:

- CPU temperature / load
- Memory / disk usage
- Uptime
- Hostname / OS / kernel
- Key systemd service status (`docker`, `NetworkManager`, `chrony`, UnitLab host agents)

## Redis contract

- Commands stream: `core_diag:commands`
- Events stream: `core_diag:events`
- State key: `core_diag:state`

Commands:

- `status`

## Install

Use the install script:

```bash
sudo ./install/install_rpi_core_diag_agent.sh
```

