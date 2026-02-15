# Live Signal Sheet + Test-Run Snapshot Pipeline

## 1) Product Rules (single source of behavior)
- В `workspace` всегда один активный `live signal sheet`.
- Импорт делается из любого Excel: маленький ad-hoc лист или проектный лист — одинаково.
- Привязки `signal -> channel` живые и изменяемые в любой момент.
- Snapshot фиксируется только в `test_run` при запуске.
- При повторном запуске используется preflight: проверка доступности участвующих модулей, reallocation, экспорт нового cable journal.

## 2) Domain Model
- `SignalSheet` (singleton на workspace): метаданные импорта + нормализованные sheet rows/headers для UI.
- `SignalSheetPreset`: сохранённые настройки импорта (`sheet/columns/hmi/type mapping`).
- `Signal` (live index): нормализованные сигналы текущего активного листа.
- `SignalAllocation`: живая текущая аллокация (`signal_id -> channel_id`).
- `TestRunSignalSnapshot`: immutable снимок только для конкретного run.

## 3) Allocation Logic
- Нормализация типов сигнала в wizard: `DI/DO/AI/AO`.
- Правило совместимости каналов:
  - `DI` сигнала аллоцируется на физический `DO` канал.
  - `DO` сигнала аллоцируется на физический `DI` канал.
  - `AI` сигнала аллоцируется на физический `AO` канал.
  - `AO` сигнала аллоцируется на физический `AI` канал.
- Автоаллокация:
  - сначала online устройства,
  - затем offline fallback,
  - без дублирования channel в рамках workspace.

## 4) Backend Pipeline
1. Добавить таблицы:
- `signal_sheets`
- `signal_sheet_presets`
- `signal_allocations`

2. Удалить legacy runtime-зависимость от:
- `signal_snapshots`
- `signal_snapshot_allocations`

3. API:
- `GET /api/v1/workspaces/{id}/signal-sheet`
- `POST /api/v1/workspaces/{id}/signal-sheet/import`
- `GET/POST/DELETE /api/v1/workspaces/{id}/signal-sheet/presets`
- `GET/PUT /api/v1/workspaces/{id}/signal-allocations`
- `POST /api/v1/workspaces/{id}/signal-allocations/auto`

4. Импорт:
- parse workbook,
- upsert live `signals`,
- deactivate stale signals (не вошедшие в текущий импорт),
- cleanup orphan allocations,
- upsert singleton `signal_sheet`.

5. Test-run snapshot:
- на `start` фиксировать snapshot из allocation entries конкретного run,
- не строить snapshot от всех signals workspace.

## 5) Frontend Pipeline
1. Store/API migration:
- `signalSheetStore` + `signal_sheet.api.ts`.
- убрать зависимость Signals screen от `signalSnapshotStore`.

2. Import wizard:
- ручной flow (upload -> columns -> hmi -> types),
- выбор preset для prefill,
- сохранение нового preset при импорте.

3. Signals grid:
- сохранить текущий datagrid UX (virtualization, resize, sort/filter, keyboard/list behavior),
- inline channel allocation select,
- control action через affino overlay (`@affino/menu-vue` / popover semantics),
- автоаллокация unassigned.

4. TestRun builder:
- preload из live allocations,
- run создаётся как раньше,
- immutable snapshot остаётся в run lifecycle.

## 6) Operational Flow
1. Инженер импортирует файл (или импортирует по preset).
2. Система формирует активный live signal sheet.
3. Инженер аллоцирует сигналы на каналы (ручно/авто), запускает control по месту.
4. Создаёт test run из нужных sequences + allocation entries.
5. На start фиксируется run snapshot.
6. На repeat/preflight система проверяет доступность модулей, предлагает reallocation, выдаёт cable journal.

## 7) Acceptance Checklist
- `/signals` работает без обращения к legacy snapshot tables.
- После импорта всегда один активный sheet в workspace.
- Переназначение channel выполняется без freeze и без дубликатов channel.
- Auto-allocate корректно учитывает DI<->DO / AI<->AO mapping.
- Test-run snapshot создаётся только при запуске run и только из учаcтвующих entries.
- Preflight/reallocate/export cable journal продолжают работать на новых аллокациях.
