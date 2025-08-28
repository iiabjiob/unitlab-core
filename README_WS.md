# 📡 UnitLab WebSocket Communication

Documentation of interaction between **Frontend (Vue/TypeScript)** ↔ **Backend (FastAPI/Python)** via **WebSocket**.  
Defines which **messages** the frontend sends to the backend and which **events** the backend broadcasts to the frontend.

---

## 📂 File Structure

### Backend
```
app/schemas/ws/
  messages.py   # incoming WS messages (Frontend → Backend)
  events.py     # outgoing WS events (Backend → Frontend)
```

### Frontend
```
src/types/ws/
  messages.ts   # message types the frontend sends
  events.ts     # event types the frontend receives
```

---

## 🔄 Communication Overview

- **Frontend → Backend**  
  WebSocket messages (`WSMessage`) described in `messages.py` / `messages.ts`.  
  Used for:
  - subscribing to channels,
  - scanning devices,
  - requesting device states,
  - sending DO/AO commands.

- **Backend → Frontend**  
  WebSocket events (`WSEvent`) described in `events.py` / `events.ts`.  
  Used for:
  - sending device states (DI/DO/AO),
  - registering new devices,
  - command responses (RESP),
  - heartbeat status (online/offline).

---

## 📋 Mermaid sequence diagrams

### 1. 🔎 Scan Devices → Register
sequenceDiagram
    participant Frontend
    participant Backend
    participant Device

    Frontend->>Backend: WS (ScanDevicesMessage)
    Backend->>Device: MQTT (SYS.SCAN)
    Device-->>Backend: MQTT (SYS.REGISTER)
    Backend-->>Frontend: WS (DeviceRegisterEvent)

### 2. 📡 Request State (DO)
sequenceDiagram
    participant Frontend
    participant Backend
    participant Device

    Frontend->>Backend: WS (RequestStateMessage { mode: REQ_ALL_BIT })
    Backend->>Device: MQTT (REQ_ALL_BIT)
    Device-->>Backend: MQTT (STATE_ALL_BIT)
    Backend-->>Frontend: WS (DeviceStateEvent { mode: STATE_ALL_BIT })

### 3. ⚡ Send DO Command
sequenceDiagram
    participant Frontend
    participant Backend
    participant Device

    Frontend->>Backend: WS (SetDoCommandMessage { mode: SET_SINGLE_BIT })
    Backend->>Device: MQTT (CMD.SET_SINGLE_BIT)
    Device-->>Backend: MQTT (SYS.RESP { status: OK, error: NONE })
    Backend-->>Frontend: WS (DeviceRespEvent)

### 4. ❤️ Heartbeat (online/offline)
sequenceDiagram
    participant Device
    participant Backend
    participant Frontend

    Device-->>Backend: MQTT (SYS.HEARTBEAT)
    Backend->>Backend: Update Redis { last_seen, status }
    Backend-->>Frontend: WS (DeviceHeartbeatEvent { status: "online" })

---

## 📨 Frontend → Backend (WS Messages)

### Actions
All actions are defined in `WSAction` enum:

```ts
export enum WSAction {
  SUBSCRIBE = "subscribe",
  UNSUBSCRIBE = "unsubscribe",
  SET_DO_COMMAND = "set_do_command",
  SET_AO_COMMAND = "set_ao_command",
  GET_STATES = "get_states",
  SCAN_DEVICES = "scan_devices",
}
```

### Message Examples
- **Subscribe**
  ```json
  { "action": "subscribe", "channels": ["devices/state"] }
  ```
- **Unsubscribe**
  ```json
  { "action": "unsubscribe", "channels": ["devices/state"] }
  ```
- **Scan Devices**
  ```json
  { "action": "scan_devices" }
  ```
- **Request State**
  ```json
  { "action": "get_states", "unit_id": "DO-123", "device_type": "do", "mode": "REQ_ALL_BIT" }
  ```
- **DO Command**
  ```json
  { "action": "set_do_command", "unit_id": "DO-123", "mode": "SET_SINGLE_BIT", "ch": 5, "value": 1 }
  ```
- **AO Command**
  ```json
  { "action": "set_ao_command", "unit_id": "AO-555", "ch": 2, "value": 12.3 }
  ```

---

## 📡 Backend → Frontend (WS Events)

### Channels (WSChannel)
```ts
export enum WSChannel {
  DEVICE_STATE    = "devices/state",
  DEVICE_REGISTER = "devices/register",
  DEVICE_RESP     = "devices/resp",
  DEVICE_STATUS   = "devices/status",
}
```

### Event Examples
- **Device State**
  ```json
  {
    "channel": "devices/state",
    "unit_id": "DO-123",
    "device_type": "do",
    "timestamp": 1693058123456,
    "mode": "STATE_ALL_BIT",
    "payload": { "bitmask": 15 }
  }
  ```

- **Device Register**
  ```json
  {
    "channel": "devices/register",
    "unit_id": "DO-123",
    "device_type": "do",
    "channels": 32,
    "firmware_version": 1,
    "is_active": true,
    "status": "online",
    "last_seen": 1693058123456
  }
  ```

- **RESP**
  ```json
  {
    "channel": "devices/resp",
    "unit_id": "DO-123",
    "device_type": "do",
    "packet_id": 42,
    "status": "OK",
    "error": "NONE",
    "timestamp": 1693058123456
  }
  ```

- **Heartbeat**
  ```json
  {
    "channel": "devices/status",
    "unit_id": "DO-123",
    "device_type": "do",
    "status": "online",
    "last_seen": 1693058123456
  }
  ```

---

## ⚙️ Usage Notes

- **messages.py / messages.ts** → models for messages frontend sends.  
- **events.py / events.ts** → models for events backend sends.  
- **protocol/modes.py** (backend) → single source of truth for command/state enums (`Cmd`, `State`, `RespStatus`, `RespError`).  
- **messages.ts / events.ts** (frontend) → mirrored enums for WS serialization/typing.  

---

## ✅ How to Use This Doc

- To see **what frontend can send** → check `messages.py` / `messages.ts`.  
- To see **what backend can send** → check `events.py` / `events.ts`.  
- To extend protocol (new action/event) → add to enums + models on both backend and frontend.  

---
