# Affino Primitives Migration Pipeline (Modal/Select/Combobox/etc.)

## Цель
Перевести все стандартные и кастомные UI-контролы (`modal`, `select`, `combobox`, `dropdown`, `tooltip`, `popover`, `menu`, `checkbox/radio/switch`) на Affino primitives без изменения текущего дизайна, поведения сценариев и UX-потока.

## Принципы
- Дизайн не меняем: только замена внутренних primitive-слоев.
- Совместимость по API компонентов: существующие пропсы/слоты/ивенты сохраняются через адаптеры.
- Миграция по вертикалям, а не «большим взрывом».
- Для каждого шага: визуальный регресс + e2e smoke.

## Phase 0. Inventory (1-2 дня)
1. Собрать реестр всех компонентов и мест использования:
- `UiModal`, `ConfirmModal`
- все `select`/`combobox` (включая `native <select>`)
- `UiMenu*`, `dropdown`, `popover`, `tooltip`
- контролы в `signals/`, `testRuns/`, `sequences/`, `switchgears/`
2. Для каждого компонента зафиксировать:
- текущий контракт (props/events/slots)
- состояния (`disabled`, `loading`, `error`, `keyboard`, `focus trap`)
- критичные сценарии (например, import wizard, allocation editor, test run builder)

## Phase 1. Compatibility Layer (2-3 дня)
1. Добавить `frontend/src/components/affino/`:
- `AffinoModalAdapter.vue`
- `AffinoSelectAdapter.vue`
- `AffinoComboboxAdapter.vue`
- `AffinoMenuAdapter.vue`
- `AffinoPopoverAdapter.vue`
2. Каждый адаптер:
- принимает старые пропсы и эмитит старые события
- внутри использует Affino primitive
- экспортирует те же CSS class hooks, чтобы не сломать текущий стиль
3. Добавить feature flag `AFFINO_PRIMITIVES_ENABLED` для безопасного переключения.

## Phase 2. High-Risk First (3-4 дня)
1. Сначала перевести сценарии с максимальной бизнес-ценностью:
- импорт сигнал-листа (`SignalImportModal`)
- allocation flow (`AllocationEditor`, live panel filters)
- test run create/edit (`TestRunBuilder`, `TestRunsTable`)
2. Для каждого экрана:
- заменить только primitive-узлы на адаптеры
- сверить keyboard/accessibility поведение
- прогнать smoke e2e

## Phase 3. Platform-Wide Rollout (3-5 дней)
1. Модульно перевести остальные страницы:
- `sequences`
- `devices`
- `switchgears`
- shared layout/actions
2. Запретить новые прямые `native <select>` и старые модалки через lint rule + checklist в PR.

## Phase 4. Hardening (1-2 дня)
1. Удалить feature flag и старые primitive-реализации.
2. Убрать мертвые стили и дублирующиеся хуки.
3. Зафиксировать финальный UI contract doc для Affino adapters.

## Тестовая стратегия
- Unit:
  - адаптеры: контракт props/events/slots
  - keyboard handling (`Esc`, `Tab`, `ArrowUp/Down`, `Enter`)
- Integration:
  - wizard шаги импорта
  - аллокация + быстрый поиск сигнала
- E2E smoke:
  - engineer happy path: import -> allocate -> create run -> start
- Visual regression:
  - baseline скриншоты до миграции и после

## Definition of Done
- 100% целевых контролов работают через Affino primitives/адаптеры.
- Нет изменений дизайна (pixel diff в допустимом пороге).
- Нет регрессий в keyboard/a11y поведении.
- Все критичные e2e сценарии проходят.

## Рекомендуемая последовательность PR
1. `PR-1`: adapters + feature flag + тестовый стенд.
2. `PR-2`: signals/testRuns migration.
3. `PR-3`: sequences/devices/switchgears migration.
4. `PR-4`: cleanup + remove legacy primitives.
