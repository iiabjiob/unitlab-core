# UnitLab Pipeline: Scoped WS + Lazy Channels

## Goal
Снизить CPU/трафик на старте и в idle-режиме:
- статусы устройств (`online/offline`) доступны глобально во всем приложении;
- состояния каналов (`devices/state`, `devices/resp`) обрабатываются только там, где это нужно;
- каталог каналов не грузится на bootstrap приложения.

## Scope
- Frontend runtime / stores / page lifecycle.
- Без изменения UI-дизайна.
- Backend protocol остаётся совместимым (без обязательных новых WS action).

## Step 1. Split WS streams by criticality
1. Global stream (always-on):
- `devices/register`
- `devices/status`

2. Scoped stream (on-demand):
- `devices/state`
- `devices/resp`

## Step 2. Introduce realtime scopes in WS store
1. Добавить unit-scopes (множество `unit_id` на scope).
2. Добавить global realtime scope (для экранов, где нужен поток по всем устройствам).
3. Экспортировать функцию фильтра:
- `shouldProcessRealtimeForUnit(unitId)`.

## Step 3. Apply filtering in event handler
1. `devices/register`: обновлять `deviceStore`, но не гидратить все каналы глобально.
2. `devices/state` / `devices/resp`: пропускать событие, если unit вне активного scope.

## Step 4. Channel loading strategy
1. На bootstrap: не вызывать `channelStore.fetchAll/ensureLoaded`.
2. Добавить `ensureDeviceChannelsLoaded(deviceId)` для загрузки каналов конкретного устройства.
3. Оставить `ensureLoaded()` только для экранов, где реально нужен весь каталог (например, sequence/switchgear editors).

## Step 5. Page lifecycle wiring
1. `devices/:id`
- при входе: `ensureDeviceChannelsLoaded(id)`
- включить realtime scope на `unit_id` устройства
- запросить актуальные states (`requestStates`)
- при выходе: очистить scope

2. `switchgears`
- включить global realtime scope на время страницы
- при выходе выключить

3. `signals` live panel
- realtime scope включается только когда панель открыта
- при закрытии/уходе со страницы выключается

## Step 6. Functional acceptance
1. Cold start:
- devices list и online/offline работают,
- channels API не вызывается глобально.

2. Devices page:
- каналы устройства подгружаются лениво,
- state/resp обновляются в realtime.

3. Non-realtime pages:
- `devices/state` не вызывает churn в `channelStore`.

4. Switchgears / live panel:
- realtime работает только в активном контексте.

## Step 7. Next iteration (optional backend hard-subscribe)
1. Добавить WS actions:
- `subscribe_units`
- `unsubscribe_units`
- `subscribe_all_states`
- `unsubscribe_all_states`

2. На backend хранить client->scope и фильтровать broadcast server-side.
3. Оставить текущий frontend scope как fallback safety layer.
