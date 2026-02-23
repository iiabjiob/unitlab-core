# UnitLab RPi Provision Agent

Host-side provisioning and smoke-check agent for UnitLab Core (Raspberry Pi 5).

## Purpose

Allows the UI (`Settings → Provisioning`) to:

- run host-side checks (Docker, compose, repo files, host agents)
- run smoke checks against local API endpoints
- execute host-agent install scripts (network / NTP / diagnostics)

## Redis contract

- Commands stream: `core_provision:commands`
- Events stream: `core_provision:events`
- State key: `core_provision:state`

## Commands

- `status`
- `smoke_check`
- `install_net_agent`
- `install_ntp_agent`
- `install_diag_agent`

