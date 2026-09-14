# Слайз C — контракт durable revision и immutable test plan

Статус: подэтапы C1/C2 и D1 реализованы, D2 (recovery/retest semantics) открыт.
Дата: 2026-09-14.

## Цель

Убрать зависимость испытания от frontend-счётчика `allocationRevision` и от текущих
данных workspace после постановки job в очередь. Запущенный прогон должен иметь
неизменяемый план, а отчёт — однозначную ссылку на этот план.

## Предлагаемая модель

### `signal_list_revisions`

Отдельная durable-сущность ревизии списка в рамках workspace:

- `id` — UUID или BIGINT durable revision ID;
- `workspace_id` — владелец;
- `revision_no` — монотонный display номер внутри workspace;
- `status` — `draft | active | superseded`;
- `source_hash`, `schema_version`, `rows_count`;
- `created_by`, `created_at`, `activated_at`;
- `content_hash` — hash канонического состава ревизии.

Ревизия immutable после `active`. Изменение сигнала или allocation создаёт новую
draft/active ревизию, а не мутирует состав использованной ревизии.

### `signal_list_revision_items`

Канонический snapshot строк, достаточный для исполнения и отчёта:

- signal identity/key/name/io direction/category;
- параметры команды и ожидаемого результата;
- protocol/IEC 61850 metadata;
- allocation ID, device ID, unit ID, channel ID/index/type и allocation metadata;
- порядок строки и исходный `live_signal_id` как необязательная lineage-ссылка.

`live_signal_id` и текущая `signal_allocations` остаются источником следующей
ревизии, но не runtime truth уже созданного test plan.

### `test_runs`

Добавить обязательную для новых прогонов ссылку `signal_list_revision_id` и
сохранить `revision_content_hash`. Для каждого запуска создать immutable
`test_run_plan`/`test_run_plan_items` либо использовать существующую snapshot-
таблицу только после проверки фактической схемы на head migration.

Минимальный runtime plan item должен хранить `run_id`, `signal_id`, порядок,
binding/device/channel, expected parameters и verification target. Evidence и
retest ссылаются на plan item, а не перечитывают текущую allocation.

## Предлагаемый API-контракт

Изменение публичного API выполнять отдельным согласованным подэтапом.

### Создание прогона

```json
{
  "signal_list_revision_id": 42,
  "signal_ids": [101, 102],
  "signal_interval_ms": 1000,
  "toggle_mode": "single",
  "verification_enabled": true
}
```

Backend атомарно проверяет, что revision принадлежит workspace, активна и содержит
запрошенные строки, затем создаёт plan snapshot. `signal_ids` после этого только
ограничивают выборку внутри revision и не являются источником текущих bindings.

Ответ job должен содержать:

```json
{
  "job_id": "...",
  "signal_list_revision_id": 42,
  "plan_content_hash": "..."
}
```

Старое поле `verification_signal_list_revision_id` не принимать как frontend
счётчик. На переходный период `null/0` допускается только для legacy запуска и
получает `revision_status: "unknown"`; backend не выдумывает revision задним
числом.

### Retest

Retest по умолчанию принимает исходный `test_run_id`/plan snapshot. Запуск по
новой ревизии требует явного `signal_list_revision_id`; в отчёте сохраняются обе
ссылки lineage.

## Правила параллельных изменений

- Редактирование signal list и rebind не меняют plan уже созданного run.
- Изменение allocation во время run не применяется к текущему воздействию.
- Новый run получает новую revision после успешной фиксации изменений.
- Если binding из revision больше недоступен, старт блокируется; runtime не
  подменяет его текущей allocation.
- Legacy evidence с неизвестной revision отображается как `unknown`, не получает
  доказанную историческую привязку.

## Статус реализации

Подэтап C1 добавил durable `signal_list_revisions` и
`signal_list_revision_items`, content hash и endpoint создания активной ревизии:

`POST /api/v1/workspaces/{workspace_id}/signal-list-revisions`

Ревизия строится сервером из текущих сигналов и allocation projection. При
создании test-run backend закрепляет revision ID (для legacy `null/0` создаётся
серверная ревизия) и сохраняет `signal_test_run_plans` вместе с plan items до
публикации job в очередь.

Worker D1 читает binding и порядок из plan items. Перед воздействием он отдельно
сверяет только свежую доступность и неизменность physical binding; при rebind
строка блокируется как `binding_changed`, текущая allocation не подставляется.
Recovery/retest semantics и legacy jobs без plan остаются открытыми.

## Порядок реализации после согласования

1. Проверить фактические таблицы и head migration; выбрать reuse или новые таблицы.
2. Добавить durable revision и immutable item schema без изменения старого API.
3. Добавить endpoint создания/активации revision и серверный snapshot plan.
4. Перевести worker на plan items пакетной загрузкой.
5. Добавить migration/backfill только с явным `unknown` для исторических запусков.
6. Удалить frontend `allocationRevision` из роли revision ID после переходного периода.

## Приёмка

- reload браузера не меняет revision ID;
- параллельный rebind не меняет уже созданный plan;
- worker не перечитывает binding из текущего workspace для каждого шага;
- отчёт восстанавливается по revision ID и plan content hash;
- legacy запуск не получает выдуманную revision.
