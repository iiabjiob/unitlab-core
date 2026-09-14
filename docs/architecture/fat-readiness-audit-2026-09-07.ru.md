# Аудит готовности UnitLab к промышленному FAT

Дата: 2026-09-07. Проверенный commit: `a5267b004afb5df45fc09c517186ec04b3014b49`.

Статус: аудит исходников завершён; перечисленные исправления **не реализованы этим документом**. Все GAP ниже открыты. Документ предназначен для подготовки отдельных implementation-промптов и отслеживания закрытия пробелов.

## Цель и заключение

Цель продукта: упростить работу наладчика — подключить периферийные модули, проверить сухие контакты шкафа за один прогон и получить подтверждение от проверяемого устройства по IEC 61850. Базисы: простота, понятность, масштабируемость, скорость.

Текущая реализация имеет полезную основу, но её нельзя считать доказанно готовой к роли доверенной автоматизированной перемычки. Главные препятствия: достоверность подтверждения, восстановление выходов при сбоях и сохранность доказательств. Производительность grid не является главным установленным риском.

Продуктовый путь, который нужно довести целиком:

`подключить → проверить готовность и привязки → явно разрешить воздействие → подтвердить замыкание → подтвердить восстановление → получить воспроизводимый отчёт`.

Ожидаемое состояние, команда в очереди, ACK периферии, наблюдение IED и итоговый verdict должны оставаться отдельными понятиями.

## Границы доказательств

- Проверены исходники frontend, backend, runtime IEC 61850, отдельных host agents и production compose; выполнены перечисленные ниже локальные тесты.
- Не проверены production-логи, реальная конфигурация установленного хоста, физическая периферия и шкаф, сетевые трассы реального прогона, браузер под нагрузкой 20k строк и работа в течение смены.
- «Подтверждено кодом» означает наличие конкретного механизма, а не воспроизведение аварии на оборудовании.
- Ошибка классификации старого наблюдения воспроизведена на уровне функций. Полный аппаратный путь не воспроизводился.
- Причина конкретных повторных тостов на production остаётся гипотезой до получения текста события и состояния служб.
- Performance-пункты без измерений обозначены как риски или пробелы в валидации. Здесь нет заявленных результатов FPS, latency или throughput.

## Что уже стоит сохранить

- Backend-исполнение вынесено в worker; имеются execution lease, курсоры, защита от некоторых повторных исполнений, preflight и отдельная диагностика verification.
- Есть PostgreSQL-evidence шагов и события изменения allocation. Проблема не в полном отсутствии истории, а в гарантиях её полноты и связи с ревизией.
- Frontend использует shallow-состояние, точечные патчи grid и batching через requestAnimationFrame.
- WebSocket имеет ограниченные очереди и таймаут отправки; необходимо исправить завершение соединения при перегрузке.
- Не нужен широкий переписанный framework, новые универсальные managers или переход к server-side grid без измерений.

## Приоритеты

- **P0:** блокирует доверенную аппаратную эксплуатацию или допускает недостоверный результат.
- **P1:** необходимо для устойчивого эксплуатационного релиза.
- **P2:** улучшение скорости и удобства после измерений; не отменяет ранний сбор baseline.

## GAP-01 — Старое наблюдение классифицируется как свежее, качество выводится из статуса

**P0 · backend verification · подтверждено кодом; возраст воспроизведён локально.**

Источник: [verification_execution.py](../../backend/app/services/verification_execution.py), функции `_latency_ms`, `_resolve_evidence_state`, `_build_step_and_evidence`.

Отрицательная задержка превращается в ноль. Наблюдение за пять минут до trigger при отсутствии diagnostics классификатор принял как `observed/live`. В evidence `quality="good"` присваивается по статусу `observed`. Само получение отчёта не доказывает качество и причинную связь с воздействием. Для latency используется время получения report; время источника требует отдельной политики.

**Порядок исправления:**
1. Добавить регрессии: report до trigger, старое поколение соединения, неизвестное/плохое качество, отсутствующее значение, несинхронные часы.
2. Разделить время получения, время источника и монотонную длительность ожидания; отвергать события до границы текущего воздействия.
3. Проверять поколение, допустимую причину report, качество и наличие ожидаемого значения; не подставлять `good` без evidence.
4. Согласовать политику часов/качества и отображать причину `blocked/invalid/stale` в отчёте.

**Приёмка:** старое/неполное наблюдение не даёт PASS; свежий допустимый report даёт PASS только при совпадении значения. Симулятор покрывает негативные случаи; реальный IED нужен для подтверждения временной семантики.

**Статус slice A (2026-09-14): частично закрыт.** Отрицательная задержка больше не нормализуется в ноль: report до trigger получает `stale/report_before_trigger` и не может дать PASS. Evidence builder использует timestamp выбранного source observation и отклоняет malformed timestamp. Остались проверка поколения соединения, source quality и монотонной длительности ожидания.

## GAP-02 — Двойной toggle не доказывает оба перехода

**P0 · test-run · подтверждено порядком операций.**

Источник: [signal_test_run_runner.py](../../backend/app/workers/signal_test_run_runner.py), DO-ветка `_handle_test_run` и вызов `capture_triggered_signal`.

Команда переключения и команда восстановления отправляются до capture; ожидаемое значение double-режима — исходное. Подтверждение исходного значения после этого не доказывает промежуточное замыкание. Trigger фиксируется после постановки команд в очередь.

**Порядок исправления:**
1. Зафиксировать конечный автомат одного контакта и ожидаемые переходы для исходных 0 и 1.
2. Установить границу наблюдения до воздействия; отдельно выполнить и подтвердить каждый переход.
3. Сохранять оба воздействия, ACK и отчёты IED как отдельные доказательства шага.
4. Связать прерывание любого этапа с восстановлением выхода, а не с обычным продолжением цикла.

**Зависимости:** GAP-01, GAP-03, GAP-04. **Приёмка:** отсутствие промежуточного перехода всегда исключает PASS, даже если финальное значение исходное; проверить задержанные, дублированные и переставленные reports.

**Статус slice G8 (2026-09-14): частично закрыт.** Для DO worker теперь каждый
переход double-toggle сопровождается отдельным request/readback barrier, а SET и
RESTORE сохраняются отдельными command intents/evidence; отсутствие любого
readback исключает успешный verdict. IEC 61850 остаётся опциональным наблюдением
и не заменяет физический readback.

## GAP-03 — Успех очереди смешан с успехом испытания; ACK не замыкает execution

**P0 · command/test-run/evidence · подтверждено кодом.**

Источники: [signal_test_run_runner.py](../../backend/app/workers/signal_test_run_runner.py), [command_queue_service.py](../../backend/app/services/command_queue_service.py), [device_resp.py](../../backend/app/infrastructure/mqtt/handlers/device_resp.py).

После enqueue выставляется success; счётчик `succeeded` и status evidence могут оставаться успешными при `not_validated` или несовпадении verification. Детальная ошибка сохраняется, но сводка противоречива. RESP передаётся в WS, а test-run не ожидает его как барьер. Команды публикуются с QoS 0; повышение QoS само по себе не заменит подтверждение исполнения.

**Порядок исправления:**
1. Разделить delivery/execution status и FAT verdict; согласовать публичное представление до изменения API.
2. Проверить идентификаторы: текущий packet ID локален процессу, correlation строки test-run повторяются между прогонами. Определить однозначную корреляцию run/attempt/step/command/device.
3. Добавить backend-ожидание ACK, timeout и явный unknown при неопределённом результате; определить допустимость retry и deduplication.
4. Пересчитывать сводки и отчёты из независимых подтверждений, не из количества enqueue.

**Приёмка:** потерянная команда, negative ACK, поздний/чужой/дублированный ACK и ошибка IED не дают успешного FAT verdict. Не переинтерпретировать старые неоднозначные evidence как доказанный PASS при миграции.

## GAP-04 — Восстановление выхода и исходное состояние не гарантированы

**P0 · peripheral safety/test-run · подтверждено кодом; аппаратная защита не проверена.**

Источник: [signal_test_run_runner.py](../../backend/app/workers/signal_test_run_runner.py), `get_unit_bitmask`, double-toggle и отмена.

При отсутствии/ошибке bitmask принимается ноль; далее используется локальный кэш. Восстановление зависит от второй команды после sleep. Падение worker или недоставка restore могут оставить выход изменённым.

**Порядок исправления:**
1. Запретить воздействие при неизвестном/устаревшем исходном состоянии; получить свежий readback.
2. Определить безопасное состояние и максимальное время воздействия для конкретного модуля/шкафа; исходное состояние не автоматически безопасное.
3. Проверить поддержку pulse/watchdog устройством. При её отсутствии выделить отдельный firmware/protocol slice; серверный finally не решает потерю питания сервера.
4. Реализовать и протоколировать abort/restore/readback; при невозможности восстановления сохранять `recovery_required` и блокировать новый запуск.

**Приёмка:** simulator fault injection между SET и RESTORE, обрыв сети и остановка worker; затем стендовая проверка автономной защиты. Без неё нельзя заявлять гарантированный safe-state.

## GAP-05 — Ревизия испытанного списка подменяется локальным UI-счётчиком

**P0 · signal-list/allocation/reports · подтверждено кодом.**

Источники: [SignalsPage.vue](../../frontend/src/pages/signals/SignalsPage.vue), [signalSheetStore.ts](../../frontend/src/stores/signalSheetStore.ts), [signal_sheet.py](../../backend/app/models/signal_sheet.py).

`allocationRevision` передаётся как `verificationSignalListRevisionId`, хотя начинается с нуля в frontend и увеличивается при обновлениях. Исполнитель читает текущую привязку перед каждым сигналом. Это не неизменяемая версия списка и плана испытания.

**Порядок исправления:**
1. Предложить модель durable revision и immutable run snapshot, их связь с существующими workspace/signal/allocation/evidence.
2. Согласовать миграцию и API; неизвестную историческую ревизию пометить явно, не выдумывать её задним числом.
3. Сохранять до запуска состав сигналов, bindings, параметры и ожидаемые результаты; исполнять этот snapshot.
4. Определить поведение редактирования/rebind во время прогона и retest с новой ревизией.

**Приёмка:** reload браузера не меняет revision ID; параллельное редактирование не меняет план уже запущенного теста; отчёт воспроизводится по сохранённой ревизии.

**Статус slice C1 (2026-09-14): частично закрыт.** Добавлены durable-таблицы
`signal_list_revisions` и `signal_list_revision_items`, серверный content hash и
endpoint создания активной ревизии. Frontend-счётчик больше не является новым
durable revision ID в этом endpoint. Immutable plan test-run, обязательная передача
revision в enqueue и перевод worker на snapshot остаются открытыми.

**Статус slice C2 (2026-09-14): частично закрыт.** При enqueue test-run backend
закрепляет revision ID, для legacy `null/0` создаёт серверную ревизию и сохраняет
`signal_test_run_plans`/items до публикации job. Worker ещё не исполняет этот
snapshot и продолжает перечитывать текущий allocation context.

**Статус slice D1 (2026-09-14): частично закрыт.** Worker теперь загружает
порядок и binding из immutable plan. Перед каждым воздействием обновляется только
fresh safety truth: доступность устройства и неизменность физической привязки.
При rebind шаг получает `binding_changed` и не переключает новый канал. Recovery,
retest lineage и legacy jobs без plan требуют отдельного подэтапа.

**Статус slice D2 (2026-09-14): частично закрыт.** Resume/retest по
`resume_job_id` теперь наследует и клонирует исходный immutable plan; переданная
другая revision отклоняется. Запуск без plan не получает fallback к текущей
allocation и блокируется. При ошибке публикации job его plan удаляется. Полная
restart reconciliation и миграция старых jobs остаются открытыми.

**Статус slice E1 (2026-09-14): частично закрыт.** Outbound DO/AO команды теперь
получают глобальный `command_id`; он сохраняется в Redis stream и возвращается в
RESP WS event при совпадении `unit_id + packet_id`. Binary/MQTT контракт не менялся.
Единый admission, durable intent, ACK barrier и cross-workspace channel lease
остаются открытыми.

**Статус slice E2 (2026-09-14): частично закрыт.** Добавлен внутренний
Redis-backed channel lease с TTL, владельцем и fencing epoch; lease не может быть
снят другим владельцем. FAT worker получает lease на время одного test-step и
блокирует шаг при занятом канале. Подключение к manual/sequence paths, durable
intent и ACK barrier ещё не выполнены.

**Статус slice E3 (2026-09-14): частично закрыт.** Добавлена PostgreSQL-таблица
`hardware_command_intents`. FAT DO/AO intent сохраняется и коммитится до outbound
publish, после успешной постановки получает `queued`; ошибка публикации оставляет
долговечный `created` для диагностики/recovery. Manual/sequence paths, delivery
deadline, ACK barrier и execution/FAT verdict ещё не реализованы.

**Статус slice E4 (2026-09-14): частично закрыт.** RESP с найденным
`command_id` теперь сохраняется в intent как `acknowledged` или `negative_ack`
при совпадении `command_id + unit_id`; неизвестные correlation не изменяют intent.
ACK barrier, timeout/late/duplicate policy и перевод FAT verdict остаются открытыми.

**Статус slice E5 (2026-09-14): частично закрыт.** FAT worker теперь ждёт
`acknowledged` для всех DO/AO-команд шага; `timeout` и `negative_ack` не дают
успешный шаг и сохраняются как `failed` evidence с явной причиной. Late/duplicate
ACK policy, manual/sequence paths и финальный verdict-level barrier ещё открыты.

**Статус slice E7 (2026-09-14): частично закрыт.** Активные hardware leases
теперь регистрируются на время FAT execution и освобождаются в cleanup при
исключении/rollback; обычный terminal release остаётся идемпотентным. Отдельная
recovery-политика после процесса, manual/sequence paths и verdict barrier ещё открыты.

**Решение по IEC 61850 (2026-09-14): опциональный контур.** IEC observation не
становится глобальным обязательным барьером: `off` и `optional` сохраняют обычный
командный тест при недоступном/reserved report, а `required` блокирует PASS без
свежего подтверждения. После failed/blocked hardware command IEC capture не
запускается.

**Статус slice F2 (2026-09-14): частично закрыт.** В double-toggle
восстановительная команда теперь имеет отдельную роль `restore` в persisted
command intent и step evidence; её ACK остаётся самостоятельным обязательным
условием успешного шага. После DO-команды worker запрашивает bitmask readback и
ждёт подтверждение ожидаемого значения; для restore добавлена одна bounded
повторная попытка
публикации с тем же idempotent `command_id`; если обе попытки не доставлены,
канал остаётся `recovery_required`. Полный abort/restore/readback recovery при
падении между командами ещё открыт.

**Статус slice F3 (2026-09-14): частично закрыт.** Ошибка публикации restore
оставляет intent в состоянии `recovery_required`; отсутствие подтверждённого
финального readback также оставляет restore intent в `recovery_required`; перед этим выполняется одна
bounded повторная попытка с тем же `command_id`, а ошибка обычной DO/AO команды
остаётся `publish_failed`. Это сохраняет явный recovery сигнал даже при rollback
основной транзакции; автоматический restore/readback recovery ещё не реализован.

**Статус slice F4 (2026-09-14): частично закрыт.** Канал с незакрытым
`recovery_required` теперь блокируется для новых FAT-шагов в том же workspace.
Это предотвращает повторное воздействие до явного recovery; снятие блокировки
после физического readback и общий recovery worker ещё не реализованы.

**Статус slice E6 (2026-09-14): частично закрыт.** Timeout теперь сохраняется
в intent как terminal execution state. Поздний или дублированный RESP не может
переписать `acknowledged`, `negative_ack` или `timeout`; отдельный diagnostic event
для таких ACK и manual/sequence paths остаются открытыми.

**Статус slice E8 (2026-09-14): частично закрыт.** Manual single-channel DO/AO
теперь передаёт `workspace_id` и `channel_id`, проходит channel lease и durable
intent, а результат доставки возвращается как `hardware_command_result`.
Multi-channel DO pair/all и sequence admission требуют отдельного атомарного
multi-lease контракта.

**Статус slice E15 (2026-09-14): частично закрыт.** Manual WS теперь получает
`devices/command-result` с delivery `queued/rejected`; frontend показывает это
отдельным toast и не смешивает admission result с device RESP.

**Статус slice E16 (2026-09-14): частично закрыт.** Test-run принимает
`verification_policy=off|optional|required`. В `required` отсутствие mapped
signal или ошибка IEC preparation блокирует шаг до hardware publish; `optional`
сохраняет командный тест без IEC confirmation.

**Статус slice E9 (2026-09-14): частично закрыт.** Single-channel AO/DO sequence
steps теперь используют общий hardware lease, durable intent и ACK barrier.
`DO_PAIR`/`DO_BITMASK` явно блокируются до реализации атомарного multi-channel
lease; recovery и финальный verdict barrier остаются открытыми.

**Статус slice E10 (2026-09-14): частично закрыт.** Admission теперь умеет
атомарно захватывать несколько channel IDs одним lease identity и fencing epoch;
при конфликте не захватывается ни один канал. Подключение pair/all к intent,
ACK и command payload остаётся следующим шагом.

**Статус slice E11 (2026-09-14): частично закрыт.** `DO_PAIR` в sequence теперь
использует атомарный multi-channel lease, один command intent с полным
`channel_ids` payload и ACK barrier. `DO_BITMASK` пока блокируется до фиксации
его resolved channel-set; manual pair/all и полноценный multi-command verdict
остаются открытыми.

**Статус slice E12 (2026-09-14): частично закрыт.** Manual pair/all теперь
передают `channel_ids`, проходят атомарный multi-channel lease и сохраняют полный
набор каналов в command intent. Workspace/channel scope проверяется до публикации;
multi-command ACK/verdict UI и sequence `DO_BITMASK` остаются открытыми.

**Статус slice E14 (2026-09-14): частично закрыт.** Manual DO/AO теперь требует
не только совпадения `unit_id/channel_id`, но и active allocation канала в
`workspace_id`; непринадлежащий workspace канал отклоняется до lease/publish.

**Статус slice E13 (2026-09-14): частично закрыт.** Sequence `DO_BITMASK` теперь
получает resolved channel-set устройства и проходит multi-channel lease, durable
intent и ACK barrier. Требуются runtime-проверки device channel inventory и
полноценный multi-command verdict UI.

**Статус slice E18 (2026-09-14): частично закрыт.** Sequence start command
теперь несёт `workspace_id`; runner проверяет link sequence к workspace и
использует этот scope для hardware intent/admission.

**Статус slice E19 (2026-09-14): закрыт.** Sequence start без положительного
`workspace_id` теперь отклоняется; fallback на случайный workspace удалён.

## GAP-06 — Синхронное ожидание IEC 61850 блокирует event loop worker

**P1 · runtime/performance · подтверждено кодом; длительности не измерены.**

Источники: [signal_test_run_runner.py](../../backend/app/workers/signal_test_run_runner.py), [verification_runtime_orchestrator.py](../../backend/app/services/verification_runtime_orchestrator.py), `capture_triggered_signal`.

В async execution вызывается синхронный wait, внутри есть `time.sleep`. Другие задачи этого процесса, включая heartbeat, задерживаются; control state проверяется после возврата.

**Статус slice F1 (2026-09-14): частично закрыт.** Optional IEC capture теперь
вызывается через `asyncio.to_thread`, поэтому ожидание report не блокирует event
loop FAT worker. Требуются отдельные thread-safety проверки runtime, измерение
heartbeat/cancel latency и управляемая отмена длительного capture.

**Порядок исправления:**
1. Измерить event-loop lag, heartbeat и latency отмены при timeout IED.
2. Проверить thread safety и владельца runtime; выбрать async adapter либо выделенный последовательный исполнитель блокирующих операций.
3. Добавить ограниченное ожидание и отмену. Отмена await вокруг thread сама по себе не останавливает аппаратную операцию.
4. Проверить lease/heartbeat во время длительного ожидания и graceful shutdown.

**Приёмка:** задержка управления укладывается в заранее согласованный бюджет при недоступном IED; один и тот же runtime не получает конкурентные небезопасные вызовы.

## GAP-07 — При перегрузке WS соединение удаляется, но транспорт не закрывается

**P1 · backend/frontend live state · подтверждено кодом.**

Источники: [ws/manager.py](../../backend/app/ws/manager.py), [websocketStore.ts](../../frontend/src/stores/websocketStore.ts).

`disconnect` отменяет sender и удаляет очередь, но не вызывает close WebSocket. Браузер может оставаться connected без новых данных.

**Порядок исправления:**
1. Воспроизвести queue full, send timeout и initial-sync failure на медленном клиенте.
2. Сделать закрытие транспорта и cleanup идемпотентными, исключить проблемы отмены текущей sender task.
3. Проверить reconnect и получение актуального snapshot; добавить обнаружение stale-потока.

**Приёмка:** медленный клиент закрывается и восстанавливает состояние, остальные продолжают работать; нет утечек sender/sync tasks. В браузере видна потеря актуальности.

**Статус slice B (2026-09-14): частично закрыт.** При queue overflow, send timeout и initial-sync failure backend теперь удаляет клиент из runtime и отдельно закрывает underlying WebSocket transport; закрытие идемпотентно на уровне менеджера. Reconnect/snapshot recovery и browser-level slow-client regression остаются для стендовой проверки.

## GAP-08 — Аппаратные WS-команды могут отправляться после reconnect

**P0 · frontend transport/backend admission · подтверждено кодом.**

Источники: [websocketStore.ts](../../frontend/src/stores/websocketStore.ts), [transportActions.ts](../../frontend/src/stores/channelStore/transportActions.ts).

Общая очередь WSMessage не имеет срока годности; onopen отправляет накопленное. В неё входят аппаратные команды. Ограничение кнопки в одном UI не является достаточным транспортным барьером.

**Порядок исправления:**
1. Регрессия: действие offline → reconnect не должно неожиданно управлять выходом.
2. Разделить read/state requests и аппаратные действия; для последних возвращать явный отказ при отсутствии связи.
3. Согласовать backend deadline/command identity и проверку владения каналом; истёкшая команда отвергается независимо от клиента.

**Приёмка:** после reconnect нет старых SET/AO действий; оператор видит, что команда не исполнена. Связь с GAP-03 и GAP-10.

**Статус slice A (2026-09-14): частично закрыт.** Frontend transport теперь не ставит DO/AO аппаратные команды в общую очередь при закрытом или connecting WS; такие команды явно отклоняются до optimistic UI/action bookkeeping. Очередь сохранена только для state/read requests. Backend deadline, command identity и admission остаются в scope GAP-03/GAP-10.

## GAP-09 — Потеря Redis и неполный коммит evidence нарушают восстановление

**P0 · persistence/deployment/recovery · подтверждено конфигурацией и кодом.**

Источники: [docker-compose.prod.yml](../../docker-compose.prod.yml), [signal_test_run_runner.py](../../backend/app/workers/signal_test_run_runner.py), [config.py](../../backend/app/core/config.py).

Redis работает без RDB/AOF, на tmpfs. Теряются очереди, locks, cursors и job state. Evidence коммитится в том числе через flush tested-at batch, по умолчанию 50 результатов: последние воздействия до коммита могут не иметь сохранённой истории после аварии. Это не означает потери всех ранее закоммиченных шагов.

**Порядок исправления:**
1. Классифицировать cache, coordination, command intent и evidence; определить durable source of truth каждого.
2. Сохранить намерение воздействия до отправки и результат короткой транзакцией; устранить зависимость критического evidence от UI-пакета tested-at.
3. Реализовать restart reconciliation: незавершённое воздействие получает unknown/recovery_required, а не автоматический retry или PASS.
4. Согласовать Redis persistence/retention и восстановление deployment. Включение AOF без reconciliation не делает исполнение exactly-once.

**Приёмка:** kill worker/Redis до отправки, после отправки, после ACK и до коммита; ни потеря доказательств без явного unknown, ни повторное воздействие не маскируются успехом.

**Статус slice G1 (2026-09-14): частично закрыт.** При replay ранее начатого
test-run worker перед явным отказом replay теперь reconciles persisted intents со
статусами `created/queued`: обычные команды получают `unknown`, restore —
`recovery_required`; число reconciled intents попадает в recovery result. Оба
состояния блокируют дальнейший FAT-шаг по каналу. Kill-сценарии на реальном
Redis/PostgreSQL и durable evidence до/после commit ещё требуют стендовой
проверки.

**Статус slice G4 (2026-09-14): частично закрыт.** Production Redis теперь
использует named volume и AOF `everysec` вместе с периодическими RDB snapshots;
это уменьшает окно потери queue/lock/cursor state при штатном restart. Это не
гарантирует exactly-once и не заменяет restart reconciliation, backup/restore
проверку и kill-тесты на deployment.

## GAP-10 — Нет доказанной общей исключительности физического выхода

**P0 · allocation/command ownership · подтверждён разрыв границ.**

Источники: [signal_sheet.py](../../backend/app/models/signal_sheet.py), [repository.py](../../backend/app/api/v1/signal_sheet/repository.py), [do_commands.py](../../backend/app/ws/actions/do_commands.py).

Уникальность allocation и поиск конфликтов ограничены workspace. Ручной WS-путь передаёт команду в queue напрямую. Workspace/job lease не эквивалентен исключительному владению физическим каналом между всеми путями управления.

**Порядок исправления:**
1. Перечислить всех отправителей: manual, sequence, FAT, recovery; отделить повторно используемую привязку от активного владения.
2. Определить единый backend admission и атомарное получение владения нужными каналами.
3. Добавить fencing/проверку поколения владельца, release и реакцию на истёкший lease.
4. Провести через эту проверку все аппаратные команды, включая ручные.

**Приёмка:** два workspace, два worker и ручной оператор не могут одновременно воздействовать на один эксклюзивный канал; старый владелец после lease loss не продолжает командовать.

## GAP-11 — Анонимный MQTT позволяет обойти управление приложения

**P0 при сетевой доступности брокера · deployment/protocol access · конфигурация подтверждена.**

Источники: [mosquitto.conf](../../config/mosquitto.conf), [docker-compose.prod.yml](../../docker-compose.prod.yml).

`allow_anonymous true`, порт 1883 опубликован. Фактическая достижимость через firewall на production в этом аудите не проверена.

**Порядок исправления:**
1. Уточнить provisioning и существующие возможности firmware по аутентификации.
2. Ввести отдельные identities backend/модулей и ACL: модуль не публикует команды другому модулю.
3. Ограничить экспозицию брокера; определить защищённый транспорт с учётом deployment.
4. Подготовить миграцию credentials и recoverable rollout для установленных модулей.

**Приёмка:** анонимная команда и чужой topic запрещены; разрешённые регистрация, телеметрия и управление продолжают работать. Не публиковать реальные секреты в репозитории/логах.

**Статус slice G6 (2026-09-14): частично закрыт.** Backend MQTT client теперь
поддерживает credentials из environment и не хранит их в коде. Сам broker пока
остаётся anonymous в текущем deployment до согласования provisioning firmware,
password file/ACL и TLS rollout; GAP-11 не считается закрытым.

**Статус slice G10 (2026-09-14): частично закрыт.** Добавлен opt-in secure
Mosquitto profile с `allow_anonymous false`, внешним password file и persistent
broker data volume. Текущий production compose намеренно не переключён
автоматически: до provisioning firmware это могло бы остановить доступ устройств.
Rollout и negative/positive topic tests описаны отдельно; ACL per-device и TLS
остаются последующими deployment/hardware checks.

## GAP-12 — Повторные diagnostics-тосты и неоднозначность сетевой ошибки

**P1 · operator UX/host diagnostics · механизм подтверждён, production-причина не установлена.**

Источники: [wsHandler.ts](../../frontend/src/services/wsHandler.ts), [coreNetworkStore.ts](../../frontend/src/stores/coreNetworkStore.ts), [diag_collector.py](../../host-services/rpi-core-diag-agent/src/unitlab_rpi_core_diag_agent/diag_collector.py).

Любая неактивная служба в списке diagnostics даёт критический тост; список включает NetworkManager, chrony и host agents. `activating/deactivating` считаются active=false. Численные аварийные значения входят в signature; её изменение пересоздаёт бессрочный тост. Неизменная signature уже дедуплицируется. Событие core_network_state напрямую toast не создаёт; ошибки GET store пишет в lastError/console.

**Порядок исправления:**
1. Снять точный текст тоста, тип WS-события, snapshot и состояние упомянутых systemd-служб. Не выдавать найденный механизм за установленную сетевую неисправность.
2. Разделить обязательные/необязательные службы и переходные/устойчивые состояния.
3. Дедуплицировать по стабильному incident ID, обновлять существующее сообщение; добавить hysteresis/debounce для flapping и явное recovery.
4. Постоянное состояние вынести в status panel; тост оставить для нового инцидента. Проверить acknowledgement/ручное закрытие и повторный реальный сбой.

**Приёмка:** десять одинаковых/численно меняющихся snapshots одного инцидента не создают поток новых тостов; настоящая новая неисправность и восстановление видны. Визуально проверить настройки сети, diagnostics и работу во время FAT.

**Статус slice B (2026-09-14): частично закрыт.** Для core diagnostics численные изменения CPU/memory/disk не меняют incident signature; inactive-service alert ограничен обязательными `docker` и `NetworkManager`, а существующий toast обновляется вместо пересоздания и теперь сохраняет актуальные численные значения. Новый incident продвигается в toast после двух одинаковых snapshots, что отсекает одиночные flapping-состояния. Local acknowledgement скрывает текущий incident до recovery/новой signature. Production incident ID, server-side acknowledgement и recovery audit остаются открытыми.

## GAP-13 — Повторная загрузка полного allocation-контекста на каждый сигнал

**P2 · backend performance · структура запросов подтверждена, время не измерено.**

Источники: [signal_test_run_runner.py](../../backend/app/workers/signal_test_run_runner.py), [repository.py](../../backend/app/api/v1/signal_sheet/repository.py), `list_allocation_rows_by_signal_ids`.

Каждый шаг повторяет загрузку сигналов, allocation, каналов/устройств и presence. На 20k строк это десятки тысяч обращений; точный query count зависит от ORM-загрузок и требует измерения.

**Порядок исправления:**
1. Измерить SQL/Redis calls на N сигналов и p95 времени подготовки/шага.
2. После GAP-05 загрузить immutable plan пакетно, не перечитывая весь domain context для каждого шага.
3. Отдельно оставить свежую проверку доступности/владения перед воздействием; не кэшировать safety truth до конца прогона.
4. Сравнить query count и latency с baseline.

**Приёмка:** подготовка масштабируется пакетами; смена доступности устройства всё ещё блокирует воздействие. Не вводить server-side grid как лечение backend execution bottleneck.

**Статус slice G2 (2026-09-14): частично закрыт.** Зафиксирован повторяемый
deterministic 20k/5k unit baseline для frontend projection, targeted patch queue,
selection и channel-owner lookup. Backend query-count/latency measurement ещё не
снят, поэтому GAP-13 не считается закрытым.

**Статус slice G9 (2026-09-14): частично закрыт.** Перед hardware I/O worker
использует лёгкий `get_execution_binding`: allocation/channel/device и свежий
presence, без построения полного `SignalAllocationRowSchema` на каждый сигнал.
Проверки revision snapshot и текущего mutable binding сохранены. Полный query
count/p95 measurement на production PostgreSQL и batch preparation ещё не сняты.

**Статус slice G16 (2026-09-14): частично закрыт.** Recovery-barrier теперь
возвращается вместе с execution binding в одном SQL-результате, поэтому worker
не делает отдельный recovery-query перед каждым binding-query. Свежий Redis
presence и повторная lease/admission-проверка сохранены. Структурный тест
подтверждает единый DB round-trip; production query-count/p95 и batch preparation
по-прежнему требуют PostgreSQL benchmark.

**Статус slice G15 (2026-09-14): частично закрыт.** Legacy worker fixtures
синхронизированы с immutable-plan, hardware admission, ACK и readback-контрактом;
worker regression file проходит `13 passed`, полный `backend/tests` — `404
passed`. Эти тесты используют deterministic doubles и не заменяют PostgreSQL/
Redis query-count measurement или стендовый hardware run.

## GAP-14 — Нет подтверждённого performance/soak baseline целевого FAT-пути

**P1 для критериев релиза; P2 для последующих оптимизаций · validation/performance.**

Источники: [useSignalGridPatchQueue.ts](../../frontend/src/pages/signals/composables/useSignalGridPatchQueue.ts), [useSignalGridRowModel.ts](../../frontend/src/pages/signals/composables/useSignalGridRowModel.ts), [verification_regression_harness.py](../../backend/app/services/verification_regression_harness.py).

Локальные contract-тесты и simulator regression полезны, но не доказывают 20k-row UX, аппаратную скорость или отсутствие роста памяти за смену. Детальное логирование команд и результатов — дополнительный I/O-риск на целевом Pi, пока без измерения. Последовательное ожидание IED ограничивает throughput; увеличение числа worker до GAP-10 опасно.

**Порядок исправления:**
1. Зафиксировать целевой Pi/браузер, 20k строк, число модулей/IED/клиентов, частоту телеметрии, bursts и длительность soak.
2. Согласовать бюджеты command→ACK, ACK→IED, abort latency, UI patch latency, event-loop lag, queue depth, памяти и long tasks.
3. Снять baseline при scrolling/editing/selection и активных updates; отдельно при offline/reconnect и с production logging.
4. Оптимизировать только измеренные hot paths; затем добавить воспроизводимый regression gate и стендовый soak.

**Приёмка:** отчёт содержит конфигурацию, p50/p95/p99, query/message rate, dropped/coalesced counters и рост памяти; отмечены simulator-only результаты. Визуально: scroll, focus, selection, pinned alignment, отсутствие blank viewport и видимость stale-state.

**Статус slice G3 (2026-09-14): частично закрыт.** Baseline-контракт и команда
прогона зафиксированы в [fat-signal-list-baseline.ru.md](../performance/fat-signal-list-baseline.ru.md);
focused harness проходит. Browser/Pi trace, memory/long-task/scroll budgets,
MQTT/ACK и сменный soak остаются обязательными release-проверками.

**Статус slice G18 (2026-09-14): deterministic baseline повторён.** На текущем
окружении harness для 20 000 строк и burst из 5 000 allocation/runtime patches
прошёл `3/3` теста за `494 ms` (Vitest test time); frontend type-check также
прошёл. Результат остаётся simulator/unit evidence и не подменяет browser/Pi,
memory/long-task, MQTT/ACK или сменный soak.

**Статус slice G19 (2026-09-14): root backend test discovery стабилизирован.**
Добавлен `pytest.ini` с `testpaths=backend/tests` и `pythonpath=backend`, поэтому
корневой прогон не пытается собирать ignored vendor-suite IEC 61850 без
локального `pyiec61850`; полный tracked backend suite прошёл `405` тестов.
Optional IEC wrapper по-прежнему запускается отдельным runner-ом из G17.

**Статус slice G20 (2026-09-14): frontend regression suite повторён.** Полный
Vitest прогон прошёл `43` test files и `219` тестов; performance harness входит
в этот результат. Это подтверждает текущие store/transport/projection contracts,
но не заменяет визуальный browser trace на Pi и сменный soak.

**Статус slice G21 (2026-09-14): frontend production build повторён.**
`npm --prefix frontend run build` завершился успешно, включая `vue-tsc` и Vite
production build. Runtime hardware, target-browser и Pi evidence остаются
вне локального окружения.

**Статус slice G22 (2026-09-14): verdict/evidence barrier усилен.** После
положительного hardware ACK, но `late`, `missing`, `not_validated` или
`value_mismatch` IEC-verdict, step evidence теперь получает `failed`; статус
`succeeded` сохраняется только для command/test statuses `tested` и `verified`.
Счётчик `succeeded` по-прежнему означает доставленные команды и не меняет
optional IEC policy, поэтому различие delivery и FAT verdict остаётся явным.

**Статус slice G23 (2026-09-14): failure-path hardware reconciliation.** При
исключении worker после создания текущего execution attempt незавершённые
hardware intents теперь переводятся в `unknown` либо `recovery_required` до
terminal `failed` job update; количество reconciled intents сохраняется в
failure result. Это закрывает окно между публикацией команды и progress cursor,
а автономная защита/kill-test стенда по GAP-04 остаются обязательными.

**Статус slice G24 (2026-09-14): ambiguous publish failure блокирует канал.**
Manual, sequence и test-run delivery paths теперь записывают `unknown` для
не-restore команды после ошибки публикации; старый статус `publish_failed` также
учитывается recovery lookup. Поэтому неопределённая доставка не может тихо
разрешить повторный hardware command на том же физическом канале.

## Дополнительный связанный риск — AO-сценарий

При следующем аппаратном slice отдельно проверить AO-ветку [signal_test_run_runner.py](../../backend/app/workers/signal_test_run_runner.py): она выбирает случайное значение 0–24. Это подтверждено кодом, но электрические единицы/допустимые диапазоны конкретного оборудования здесь не установлены. Не переносить такой сценарий автоматически в доверенный test plan: сначала определить тип/диапазон канала, затем сохранить детерминированную последовательность значений в ревизии плана и проверить readback. Основная цель текущей очереди — сухой контакт; расширение AO оформить отдельным scope.

**Статус slice G5 (2026-09-14): частично закрыт.** AO-команда теперь также
ждёт подтверждённый float readback и не может дать успешный verdict при timeout;
неподтверждённый AO intent получает `recovery_required`. Диапазоны/единицы и
детерминированный безопасный restore AO всё ещё требуют отдельного согласования
с hardware profile.

**Статус slice G13 (2026-09-14): частично закрыт.** Автоматический test-run
больше не отправляет implicit random AO `0..24`: при отсутствии согласованного
hardware profile шаг получает `ao_profile_required` до lease/enqueue. Manual AO
остаётся явным операторским путём; диапазон и safe restore для него также должны
быть проверены hardware policy.

**Статус slice G7 (2026-09-14): частично закрыт.** Провал DO readback теперь
помечает исходный command intent как `recovery_required` и блокирует канал, в том
числе для single-toggle; это предотвращает повторное воздействие после
неподтверждённого физического состояния.

**Статус slice G11 (2026-09-14): частично закрыт.** Отсутствующий или
невалидный Redis bitmask больше не преобразуется в `0`: worker сохраняет явное
`initial_state_unknown` и не получает lease/не публикует DO-команду. Требуется
стендовая проверка свежести state snapshot и автономной защиты при потере Redis.

**Статус slice G12 (2026-09-14): частично закрыт.** Исправлен manual AO
boundary: single-channel AO теперь передаёт channel scope в общем формате
`channel_ids`, поэтому валидная команда доходит до admission/enqueue. ACK и
readback для manual-команд остаются отдельным runtime gap.
Recovery lookup для физического канала теперь не ограничен workspace: `unknown`
или `recovery_required` из другого workspace также блокирует повторное воздействие.
Timeout ожидания hardware ACK теперь переводит обычную команду в `unknown`, а
restore-команду — в `recovery_required`; оба состояния учитываются при проверке
повторного допуска канала, включая ещё не завершённые `created/queued` intents.
Это закрывает только серверный safety-barrier; kill/restart и реальный ACK-path
на стенде всё ещё требуют проверки.
В sequence admission исправлена передача fencing epoch атомарного lease в intent;
sequence hardware step больше не падает из-за обращения к несуществующему `lease`.
Manual multi-channel scope теперь отклоняет дубли каналов до atomic lease, поэтому
persisted `channel_ids` не расходится с фактически защищённым набором.
Manual DO/AO теперь также проверяет heartbeat status устройства перед получением
lease; отсутствующий или не-`online` status не допускает аппаратную публикацию.
State handler больше не собирает single-bit/changed-bit snapshot из fallback `0`,
если полный bitmask отсутствует или невалиден; это сохраняет неизвестное состояние
до полноценного state response.
Проверка дополнена TTL-backed `last_seen`, поэтому stale `online` при остановленном
offline-checker также не считается доступностью.
Успешная публикация manual intent теперь также фиксируется как `queued` до
освобождения lease; это устраняет ложный `unknown` при restart reconciliation.
Core diagnostics toast теперь имеет локальное acknowledgement: оператор может
скрыть текущий incident, а повторное уведомление появится только после recovery
или изменения нормализованной signature. Это не заменяет серверный incident ID
и audit trail.
В verification evidence builder теперь используется timestamp выбранного source
observation, а не только время получения transport event; некорректный source
timestamp не может замаскироваться receiver time и дать свежий PASS.
Post-command DO readback также не принимает отсутствующий или невалидный snapshot
за подтверждённый `0`; такие состояния доходят до timeout/recovery.
После failed DO readback worker больше не использует локальный predicted bitmask
для следующих DO-каналов того же устройства в рамках run: устройство получает
`initial_state_unknown` до завершения run.

**Статус slice G17 (2026-09-14): validation-only для optional IEC 61850.**
Добавлен tracked runner `scripts/test-optional-iec61850-wrapper.sh`: при отсутствии
локально собранного `pyiec61850` он явно сообщает `SKIP` и завершает проверку
успешно; при наличии dependency запускает внешний wrapper test. Это не заменяет
full wrapper/hardware gate и не делает IEC 61850 обязательным для обычного FAT.

## Рекомендуемая последовательность слайсов

Это предложение порядка, не разрешение на изменение всех API/протоколов одновременно. Номер GAP стабилен и не задаёт хронологию.

| Очередь | Фокус | Зачем и зависимости |
| --- | --- | --- |
| A | GAP-01: регрессии freshness/verdict; GAP-08: запрет отложенных воздействий | Малые изменения, устраняющие конкретные опасные допуски; согласовать любые изменения контракта |
| B | GAP-07 и GAP-12 отдельными слайсами | Устойчивость live-state и устранение потока уведомлений; диагностику production-тоста можно начать сразу |
| C | GAP-05, GAP-03, GAP-09: сначала согласование модели | Зафиксировать revision, command identity, evidence и recovery до сборки нового execution flow |
| D | GAP-05 и GAP-09: durable plan и intent/evidence | Миграции и recovery по частям, с backward compatibility |
| E | GAP-10 и GAP-03: command admission, владение, ACK, verdict | Единые гарантии для manual/sequence/FAT; не плодить параллельные владельцы |
| F | GAP-06, GAP-04, GAP-02 | Неблокирующее управляемое ожидание → гарантии восстановления → оба подтверждённых перехода |
| G | GAP-11 | Отдельный deployment/provisioning track; обязателен до аппаратного допуска в доступной сети |
| H | GAP-13 и GAP-14 | Baseline GAP-14 собрать рано; оптимизировать после стабилизации semantics и подтвердить soak |

P0 с аппаратной/firmware зависимостью нельзя закрывать только mock-тестом. Быстрые UX-исправления не означают готовность к промышленному FAT.

## Шаблон контекста для ChatGPT: подготовка одного slice-промпта

Передавать этот документ целиком либо выбранный GAP вместе с зависимостями и текущим статусом. Просить подготовить **один ограниченный slice**, не весь roadmap.

```text
Контекст: docs/architecture/fat-readiness-audit-2026-09-07.ru.md.
Закрываем GAP-XX, подэтап N. Остальные GAP остаются вне scope.
Уже закрытые зависимости: ...; незакрытые зависимости: ... .

Подготовь implementation-промпт для Codex:
1. Конкретная проблема и наблюдаемый результат с before/after примером.
2. Владелец поведения и минимальные затрагиваемые подсистемы/файлы.
3. Инварианты: аппаратная безопасность, revision, ACK, evidence, retry/reconnect.
4. Границы scope и зависимости; сначала перепроверить утверждения аудита по текущему коду.
5. Если меняется публичный API/протокол/схема — отдельное предложение контракта
   и согласование до реализации; миграция и совместимость существующих данных.
6. Негативные тесты, минимальная валидация, simulator/hardware разделение.
7. Критерии приёмки и, при UI-влиянии, краткий visual checklist.
8. Обновление статуса GAP: что закрыто, доказательства, что осталось.
Не предлагай app-local workaround для дефекта owning package.
Не закрывай весь GAP, если slice покрывает только один подэтап.
```

## Выполненная валидация исходного аудита

Backend, из `backend/`:

```bash
.venv/bin/python -m pytest -q tests/services/test_verification_runtime_orchestrator.py tests/services/test_verification_execution.py tests/services/test_verification_network_preflight.py tests/services/test_core_network_service.py
# 35 passed
.venv/bin/python -m pytest -q tests/workers/test_signal_allocation_runner_results.py tests/services/test_verification_regression_harness.py
# 19 passed
```

Frontend, из `frontend/`:

```bash
npm test -- src/api/http.test.ts src/pages/signals/composables/useSignalGridPatchQueue.test.ts src/pages/signals/composables/useSignalGridRowModel.test.ts src/stores/signalSheetStore.test.ts
# 13 passed, 4 files
```

Дополнительно исходный вызов `_latency_ms` и `_resolve_evidence_state` с `observed_at = triggered_at - 5 minutes`, допустимым report path и пустыми diagnostics до slice A возвращал `0` и `('observed', 'live', 'report_received', 'report_observation')`. После slice A добавлена регрессия: возвращается отрицательная задержка и `('stale', 'stale', 'report_before_trigger', 'report_observation')`.

Зелёные тесты описывают покрытые контракты, а не отсутствие перечисленных gaps. Full suite, build, 20k benchmark, browser visual verification и физический FAT в исходном аудите не выполнялись. Сохранение этого документа не меняет поведение приложения.

## Как обновлять статус

Для каждого завершённого slice добавить к соответствующему GAP: дату, commit/PR, закрытые подэтапы, выполненные проверки и остаточные риски. Статусы: `открыт → частично закрыт → закрыт` либо `ожидает аппаратной проверки`. Не стирать исходный finding и не называть запланированный контракт реализованным.
