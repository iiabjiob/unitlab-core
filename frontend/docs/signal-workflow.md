# Signal Snapshot → Allocation → Test Run UX

## Цели инженера
- Подготовить данные сигналов (CSV) и закрепить их за workspace.
- Настроить соответствие сигналов каналам (allocation mapping).
- Запустить тестовые прогоны на основе фиксации сигналов и видеть историю запусков.

## Основные сущности
1. **Signal Snapshot** — загруженный CSV с набором сигналов, статусы `draft` / `locked`.
2. **Allocation** — JSON mapping между строками снапшота и каналами устройств.
3. **Test Run** — зафиксированный запуск с конкретным snapshot + allocation.

## Навигация
- Внутри карточки workspace добавляем новый раздел `Signals` (табы: `Snapshots`, `Allocations`, `Test Runs`).
- Верхний subheader показывает текущий workspace + быстрые CTA (Import CSV, Run Test).

## Пользовательский поток
1. **Import Snapshot**
   - Кнопка `Import CSV` в табе `Snapshots`.
   - Диалог: выбор файла, превью заголовков, опциональный комментарий.
   - После успешной загрузки запись появляется в таблице (`status`, `rows`, `source`).
2. **Review Snapshot**
   - Клик по строке открывает drawer с информацией: поле `data` в виде таблицы (paginate), кнопка `Lock snapshot`.
   - Лок запрещает редактирование и требуется перед запуском теста.
3. **Configure Allocation**
   - Из таблицы `Snapshots` кнопка `Configure allocation` (доступна когда snapshot в draft или locked, но при locked mapping только для чтения).
   - Экран с двумя таблицами: слева строки CSV, справа каналы устройства. Drag&drop или dropdown для сопоставления. Сохраняем через API `/allocation`.
4. **Run Test**
   - В табе `Test Runs` кнопка `New test run`: выбираем snapshot (только locked) и sequence.
   - После запуска запись появляется в истории со статусами `created/running/completed/failed`.
5. **Repeat / Inspect**
   - Для каждой записи доступны действия `Repeat`, `View logs`. Drawer показывает allocation snapshot и таймлайны.

## Таблицы и UI детали
- **Snapshots table**: columns `Name/Source`, `Rows`, `Status`, `Updated`, `Actions`.
- **Allocation editor**:
  - Chips/Tags для каналов, валидация уникальности каналов.
  - Доп. поле `meta` (JSON-editor) на уровне строки mapping.
- **Test Runs**:
  - Timeline компонент для статусов + прогресс, realtime обновление через WS (reuse sequence events).

## API взаимодействия
| Действие | Endpoint |
| --- | --- |
| Список снапшотов | `GET /workspaces/{id}/signal-snapshots` |
| Импорт CSV | `POST /workspaces/{id}/signal-snapshots/import` |
| Удаление | `DELETE /signal-snapshots/{id}` |
| Заблокировать снапшот | `POST /signal-snapshots/{id}/lock` |
| Получить allocation | `GET /signal-snapshots/{id}/allocation` |
| Сохранить allocation | `PUT /signal-snapshots/{id}/allocation` |
| Создать тест-ран | `POST /workspaces/{id}/test-runs` *(добавить)* |
| Повторить тест-ран | `POST /test-runs/{id}/repeat` *(добавить)* |
| История | `GET /workspaces/{id}/test-runs` |

## Ошибки и валидация
- Показать toast при `InvalidCSVError`, `SnapshotLockedError`.
- Заблокировать кнопку "Run" пока нет locked snapshot с заполненным allocation.
- Подсветить несопоставленные сигналы / дубликаты каналов.

## Open questions
1. Нужны ли версии allocation (история)? Пока берём последнюю.
2. Нужно ли разрешать редактировать mapping после lock? Текущее правило — нет.
3. Где хранить названия snapshot (filename или alias?). Можно добавить поле `display_name` позже.

## Следующие шаги
1. Добавить недостающие тест-ран эндпоинты (POST list/repeat).
2. Создать Pinia store `useSignalSnapshotStore`, `useAllocationStore`, `useTestRunStore`.
3. Реализовать UI компоненты согласно потокам выше.
