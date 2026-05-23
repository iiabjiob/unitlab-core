# Affino DataGrid: простой продуктовый обзор для инженеров

Дата аудита: 2026-05-20

## Коротко

Affino DataGrid - это не просто таблица для вывода строк. Это готовая основа для продуктов, где таблица становится главным рабочим экраном: аналитика, внутренние инструменты, dashboards, back-office, планирование, финансовые формы, spreadsheet-like интерфейсы и большие серверные наборы данных.

Главная идея пакета: дать инженеру таблицу, которая уже думает о производительности, состоянии, редактировании, undo/redo, виртуализации, формулах, фильтрах, группировках, pivot-режиме и серверных данных. То есть команда может быстрее собрать рабочий интерфейс уровня "мини-Google Sheets внутри приложения", не изобретая свой табличный движок.

## Для кого это

Affino DataGrid хорошо подходит, если у вас:

- много строк или широкие таблицы;
- пользователи активно редактируют данные прямо в таблице;
- нужны сортировка, фильтры, группировки, агрегации или pivot-представления;
- важны стабильные keyboard/clipboard сценарии;
- нужно сохранять состояние таблицы: колонки, фильтры, selection, viewport;
- данные приходят с сервера, а не лежат целиком в браузере;
- Vue-приложение должно получить готовый `<DataGrid />`, а не набор низкоуровневых примитивов.

Если нужна только маленькая read-only таблица на 20 строк, пакет может быть избыточен. Его ценность раскрывается там, где таблица является полноценным рабочим инструментом.

## Что получает инженер

### Готовый Vue-компонент

Для большинства приложений основной вход - `@affino/datagrid-vue-app`. Он дает декларативный компонент `DataGrid`: передаете строки, колонки и настройки, получаете виртуализированную таблицу с selection, keyboard navigation, сортировкой, фильтрацией, resizing и встроенным UI-поведением.

Это снижает цену входа: не нужно вручную собирать scroll sync, overlay alignment, focus restore, resize handles, clipboard flow и визуальное состояние ячеек.

### Headless-ядро для сложных интеграций

`@affino/datagrid-core` отвечает за модель данных, проекции, события, состояние, selection, viewport math, row models и стабильный `DataGridApi`.

Это важно для команд, которым нужен не только готовый компонент, но и контролируемая архитектура: свой renderer, свой backend protocol, свои плагины, свои тесты и predictability под нагрузкой.

### Производительность как часть дизайна

В документации видно, что производительность не добавлена "потом". Она заложена в архитектуру:

- вертикальная виртуализация включена по умолчанию в app-layer;
- горизонтальная виртуализация доступна для широких таблиц;
- есть worker-owned режим для нагрузок, где UI thread не должен блокироваться;
- есть server-side/data-source режим для больших удаленных датасетов;
- есть perf gates, benchmark harness и бюджеты по scroll latency, selection drag, overlay, memory churn и browser-frame сценариям.

Практически это означает: пакет проектировался для таблиц, где пользователь скроллит, выделяет, редактирует и фильтрует не демо-данные, а реальные рабочие объемы.

### Spreadsheet-like UX

Affino DataGrid закрывает много привычных сценариев из spreadsheet-интерфейсов:

- cell/range selection;
- fill handle и drag-fill;
- copy/cut/paste;
- inline editing;
- undo/redo через history facade;
- placeholder rows, которые выглядят как пустой хвост таблицы, но не создают реальные записи до первого действия;
- формулы с выражениями, функциями, массивами, lookup helpers и incremental recompute.

Это не полный Google Sheets clone. Например, A1-style references и `A1:B10` range syntax не заявлены как часть текущего formula contract. Но для product tables, dashboards и data-entry экранов возможностей уже много.

### Аналитические сценарии

Пакет покрывает не только CRUD-таблицу:

- sort/filter/group/pagination;
- aggregation;
- tree data;
- pivot rows/columns/values;
- quick filter;
- Gantt entrypoint;
- formatting для чисел, валют и дат;
- unified state для saved views и восстановления таблицы.

Для инженера это удобно: многие вещи, которые обычно расползаются по feature flags и локальным состояниям, здесь имеют общий runtime и документированные границы.

### Серверные данные без самодельного протокола

В `docs/server-datasource/` есть отдельный integration kit для backend-owned таблиц: protocol, UX contract, consistency, frontend/backend templates и checklist.

Это сильная часть продукта. Она показывает, что таблица рассчитана не только на массив `rows` в браузере, но и на реальные серверные сценарии: pull/push, invalidation, backpressure, revisions, cache windows и placeholder rows во время загрузки.

## Почему это выглядит зрелым

По документации пакет отличается от обычной UI-библиотеки несколькими признаками:

- четкие package boundaries: core, orchestration, Vue adapter, Vue app layer, server adapters;
- стабильный namespaced `DataGridApi`, а не хаотичный набор методов;
- разделение stable, advanced и internal entrypoints;
- контрактная документация по events, state, selection, history, accessibility, clipboard, virtualization и server datasource;
- отдельные performance gates и quality baselines;
- миграционные правила и публичный API inventory.

Для среднего инженера это означает меньше сюрпризов. Есть куда смотреть, когда нужно понять, кто владеет поведением: core, adapter, app layer или backend.

## Как выбрать режим

Простое правило:

| Сценарий | Что выбрать |
| --- | --- |
| Обычная таблица, небольшой или средний объем | `main-thread` |
| Много интерактивных edits, patch storms, активные sort/filter/group | `worker-owned` |
| Большой удаленный датасет, backend-owned query/filter/pivot | `server-side` / data-source row model |
| Нужен быстрый Vue-start | `@affino/datagrid-vue-app` |
| Нужна своя оболочка или renderer | `@affino/datagrid-vue` + `@affino/datagrid-core` |

## Сильные стороны

- Быстрый старт для Vue через готовый `DataGrid`.
- Сильное headless-ядро для команд, которым нужна контролируемая интеграция.
- Виртуализация строк и колонок, включая pinned panes.
- Spreadsheet-like сценарии: selection, fill, clipboard, editing, history.
- Формулы и аналитические функции не вынесены в демо, а имеют отдельный engine boundary.
- Серверный datasource описан как production integration path.
- Есть performance/quality gates, а не только маркетинговое обещание "fast".
- Публичные API и package boundaries хорошо документированы.

## Что важно честно учитывать

- Это мощный пакет, поэтому он сложнее простых table-компонентов.
- Часть enterprise-сценариев помечена как partial или planned: например некоторые mobile/touch validation paths, screen-reader device validation, 1M-row browser guarantees и 10k-column browser guarantees.
- Для серверных таблиц предпочтительный путь - `dataSourceBackedRowModel`; `serverBackedRowModel` описан как более простой/compatibility path с ограничениями.
- Полная accessibility/WCAG готовность не заявлена без ручной assistive-technology проверки.
- Formula engine силен для field/computed expressions, но это не Excel-compatible language surface.

## Продуктовое позиционирование

Affino DataGrid можно описывать так:

> Affino DataGrid - это высокопроизводительная таблица для Vue и headless-интеграций, созданная для data-heavy продуктов. Она сочетает spreadsheet-like UX, виртуализацию, аналитические проекции, формулы, сохранение состояния и серверные datasource-контракты в одной архитектуре.

Или еще проще:

> Это таблица для приложений, где пользователи не просто смотрят данные, а работают с ними.

## Рекомендуемый pitch для инженера

Если вы строите продукт, где таблица является главным рабочим интерфейсом, Affino DataGrid экономит месяцы инфраструктурной работы. Он уже закрывает сложные части, которые обычно ломаются под нагрузкой: виртуальный scroll, selection overlays, clipboard, inline editing, undo/redo, стабильные события, состояние таблицы, формулы, pivot/group/filter pipeline и server-side data flow.

При этом пакет не заставляет вас жить только в одном UI-слое. Можно взять готовый Vue-компонент, можно использовать headless runtime, можно уйти в worker-owned режим, можно подключить серверный datasource. Это делает его не просто компонентом, а табличной платформой для инженерных команд.

## Рекомендуемый следующий шаг

Для оценки продукта стоит начинать с `@affino/datagrid-vue-app` и собрать реальный экран на своих данных: 10-20 колонок, сортировка, фильтр, resize, selection, paste и сохранение состояния. Если после этого упираетесь в объем данных или latency, переходите к worker-owned или server-side режиму по documented path, а не через локальные workarounds.

