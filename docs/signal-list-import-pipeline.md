# Signal List Import Pipeline (Demo-First, без сохранения копии Excel)

## 1. Что уже есть в текущей структуре

### Frontend (готовый каркас)
- Wizard импорта уже есть: `upload -> columns -> hmi -> types`.
- Есть сущности/сторы для `signal snapshots`, `allocation`, `test runs`.
- Есть экран live signals с быстрым `ON/OFF` по привязанным каналам.
- Есть сценарий создания test run с выбором sequence + allocation.

### Backend (текущий гэп)
- В активном `backend/app` нет рабочих API для `signal-snapshots/signals/test-runs`.
- Sequencer есть и зрелый, но прямые ссылки на `signal_key` пока не поддержаны.

Вывод: правильный путь — не изобретать новый движок тестов, а достроить доменный слой signal list и связать его с текущим sequence pipeline.

---

## 2. Принципы дизайна (под вашу задачу)

1. Signal list опционален.
2. Загруженный Excel не источник истины.
3. Файл как бинарник не храним.
4. Храним только рабочий индекс сигналов для UX, аллокации и test run.
5. Тесты исполняются через существующий sequencer.
6. UX «минимум действий»: инженер подключился к AP -> импорт -> аллокация -> run.

---

## 3. Предлагаемая модель данных

## 3.1 WorkspaceSignalCatalog (метаданные импорта)
- `id`
- `workspace_id`
- `catalog_name` (например filename)
- `source_hash` (fingerprint содержимого)
- `import_profile` (mapping колонок/настройки импорта)
- `rows_total`
- `created_at`, `updated_at`
- `is_active`

Назначение: версия рабочего набора сигналов в workspace.

## 3.2 WorkspaceSignal (рабочий индекс, не копия файла)
- `id`
- `catalog_id`
- `workspace_id`
- `signal_key` (уникальный ключ сигнала в рамках workspace)
- `display_name` (читаемое имя)
- `io_direction` (`DI|DO|AI|AO|UNKNOWN`)
- `search_text` (нормализованная строка для быстрого поиска)
- `attrs` (jsonb, только нужные инженеру атрибуты)
- `tested_at` (последнее успешное достижение до периферии)
- `created_at`, `updated_at`

Назначение: быстрый поиск, фильтрация, отображение, связь с каналами и test run.

## 3.3 SignalChannelBinding (персистентная аллокация)
- `id`
- `workspace_id`
- `signal_id`
- `channel_id`
- `binding_mode` (`direct`, `inverted`, `pulse`, `analog`)
- `default_value` (опционально)
- `updated_at`

Уникальности:
- `signal_id` unique (один сигнал -> один канал в базовой модели)
- `channel_id` unique (один канал -> один сигнал) для простоты demo

## 3.4 TestRun (на базе sequence)
- существующая сущность test run +
- `sequence_ids`
- `signal_ids[]` или `signal_filter`
- snapshot bindings на момент старта
- execution result / meta

Важно: в test run хранится snapshot привязок и сигналов на момент запуска (immutable след прогона).

---

## 4. Пользовательский шаг-за-шагом pipeline

## Шаг 0. Выбор workspace
- Без workspace импорт и тесты недоступны.

## Шаг 1. Import Excel (любой формат)
- Пользователь загружает `.xls/.xlsx/.xlsm`.
- Wizard помогает выбрать:
  - лист,
  - колонки,
  - колонку HMI имени,
  - маппинг vendor type -> `DI/DO/AI/AO`.

## Шаг 2. Normalization preview (Affino DataGrid)
- Показать нормализованные строки.
- Подсветить ошибки валидации по строкам:
  - пустой key,
  - дубликат key,
  - нераспознанный тип.
- Фильтры: `valid only`, `duplicates`, `unknown type`.

## Шаг 3. Commit в рабочий индекс
- Сохраняем только нормализованный индекс `WorkspaceSignal`.
- Исходный бинарный файл и полный sheet payload не сохраняем.
- Старый активный каталог деактивируем, новый делаем active.

## Шаг 4. Allocation signals -> channels
- Инженер сопоставляет сигналы с физическими каналами.
- Массовые операции: auto-map по имени/суффиксу, clear, confirm.
- Сохраняем как `SignalChannelBinding`.

## Шаг 5. Быстрый runtime режим
- В signal grid поиск по имени/ключу.
- Для привязанных DO/АО: кнопки `ON/OFF`/set value прямо из списка.
- Это покрывает сценарий «клиент попросил срочно включить конкретный сигнал».

## Шаг 6. Create Test Run (one-click)
- Пользователь выбирает:
  - набор sequence,
  - набор сигналов (все/фильтр/выбранные),
  - опционально заметки.
- Система фиксирует snapshot сигналов + binding на старт.

## Шаг 7. Execute через sequencer
- Test run orchestration вызывает текущий sequence pipeline.
- На этапе старта run происходит разрешение `signal -> channel`.
- В sequencer уходит уже channel-resolved команда.

## Шаг 8. Результат и tested_at
- Если команда дошла до периферии (ACK/успешная доставка в ваш контур), ставим `tested_at` для соответствующего сигнала.
- В отчете видно:
  - tested / not tested,
  - когда и каким run,
  - связь с sequence/log.

---

## 5. Как правильно переиспользовать текущий sequencer

Рекомендация для простоты:
- Не менять core `SequenceRunner` под `signal_key` на первом этапе.
- Добавить слой `TestRun Orchestrator` перед sequencer:
  1. Берет выбранные sequence.
  2. Применяет текущие `SignalChannelBinding`.
  3. Строит channel-resolved execution plan.
  4. Запускает существующие команды start/stop.

Почему это лучше:
- минимальный риск,
- reuse уже отлаженного execution/WS/event pipeline,
- быстрее вывести demo-сценарий «whole cabinet in minutes».

---

## 6. API-дизайн (без реализации кода)

## Import
1. `POST /api/v1/workspaces/{id}/signal-catalogs/parse`
- вход: файл + import profile draft
- выход: preview rows + validation issues + detected schema

2. `POST /api/v1/workspaces/{id}/signal-catalogs/commit`
- вход: normalized rows + profile
- эффект: upsert `WorkspaceSignal`, activate catalog

## Allocation
3. `GET /api/v1/workspaces/{id}/signal-bindings`
4. `PUT /api/v1/workspaces/{id}/signal-bindings`

## Quick control
5. `POST /api/v1/workspaces/{id}/signals/{signalId}/toggle`
- resolver берет channel binding и отправляет DO/AO команду

## Test runs
6. `POST /api/v1/workspaces/{id}/test-runs`
7. `POST /api/v1/test-runs/{runId}/start`
8. `POST /api/v1/test-runs/{runId}/stop`
9. `GET /api/v1/test-runs/{runId}`

---

## 7. Роль Affino DataGrid в pipeline

Использовать `@affino/datagrid-vue` в трех местах:
1. Import Preview Grid (валидация и нормализация).
2. Allocation Grid (signal -> channel с массовыми операциями).
3. Runtime Signal Grid (поиск + quick ON/OFF + tested_at).

Минимальный набор возможностей:
- virtual rows/columns,
- быстрый фильтр/поиск,
- inline status badges,
- bulk edit/select.

---

## 8. Этапы внедрения

## Phase A (MVP demo)
- backend import/commit + signal index
- allocation CRUD
- quick ON/OFF из signal grid
- test run через sequence reuse
- tested_at на ACK

## Phase B
- авто-аллокация по правилам
- richer validation profiles
- diff between catalogs (что изменилось между версиями листа)

## Phase C
- внешняя интеграция с проектным master signal list (read-only sync)
- расширенные отчеты для FAT/SAT

---

## 9. Definition of Done для вашей цели

1. Инженер загружает любой Excel и за 1-2 минуты получает рабочий signal grid.
2. Поиск сигнала в гриде и ON/OFF работает в 1-2 клика.
3. Test run стартует на базе текущих sequence и binding без ручной подготовки.
4. После успешного достижения периферии `tested_at` обновляется.
5. В системе не хранится бинарная копия Excel и не используется логика «Excel как источник истины».

---

## 10. Уточнение под реальную FAT-практику (динамика юнитов)

Система должна поддерживать два режима одинаково:
1. «На коленке» лист на 10 сигналов.
2. Полный проектный лист.

Ключевое правило:
- тест всегда ре-исполняемый,
- аллокация динамически переопределяемая между прогонами.

Рекомендуемая модель:
1. `test_run` хранит `source_test_run_id` и `allocation_revision`.
2. Любая переаллокация создает новую ревизию test run (а не мутирует старый прогон).
3. Перед `start` выполняется `preflight`:
- проверка доступности юнитов/каналов, участвующих в текущей аллокации;
- если есть недоступные, вернуть список рекомендуемых доступных каналов для каждого сигнала.
4. Для выезда в шкаф сразу доступен `cable journal export` по текущей аллокации.

Практический API-поток:
1. `GET /api/v1/test-runs/{runId}/preflight`
2. если `ready=false`:
- `POST /api/v1/test-runs/{runId}/reallocate` (создает новую ревизию с новыми каналами)
- `GET /api/v1/test-runs/{newRunId}/cable-journal/export`
3. `POST /api/v1/test-runs/{newRunId}/start`
