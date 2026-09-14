# Слайз E — единый admission и ACK для аппаратных команд

Статус: E1/E2/E3/E4/E5/E6/E7 частично реализованы; manual/sequence paths остаются открыты.
Дата: 2026-09-14.

## Проблема

Сейчас ручной WS-путь вызывает `enqueue_do_command`/`enqueue_ao_command`
напрямую. Sequence и FAT worker вызывают те же queue-функции независимо. У
команды нет общего backend admission, физического lease, явной связи
`workspace/run/step/attempt`, durable intent и барьера по RESP. `enqueue` не
означает, что модуль получил или исполнил воздействие.

## Предлагаемый command envelope

Внутренний и persisted envelope аппаратного воздействия:

```json
{
  "command_id": "cmd-uuid",
  "workspace_id": 7,
  "owner_kind": "manual|sequence|fat|recovery",
  "owner_id": "run-or-session-id",
  "step_id": "step-42",
  "attempt_no": 1,
  "device_id": 12,
  "unit_id": "UNIT-1",
  "channel_ids": [101],
  "action": "set_do|set_ao|restore",
  "payload": {},
  "created_at": "...",
  "deadline_at": "...",
  "fencing_epoch": 3
}
```

`command_id` — глобальный UUID, не packet ID. Packet ID остаётся полем
протокола и не используется как durable correlation key.

## Admission

### Реализовано в E1

Backend outbound commands теперь получают глобальный `command_id`, который
проходит через Redis stream и связывается с RESP по `(unit_id, packet_id)`.
Binary frame и MQTT topic не изменены. Это только корреляционный фундамент:
`command_id` пока не является доказательством ACK и не заменяет admission lease.

E2 добавил внутренний Redis-backed `HardwareCommandAdmission` для exclusive
channel lease с `owner_kind`, `owner_id`, TTL и monotonic `fencing_epoch`.
FAT worker теперь удерживает lease на время одного test-step и блокирует шаг при
занятом канале. E3 добавил PostgreSQL `hardware_command_intents`: FAT DO/AO
намерение фиксируется и коммитится до публикации, затем получает статус `queued`;
при ошибке публикации остаётся видимым как `created`. E4 сохраняет сопоставленный
RESP как `acknowledged`/`negative_ack` в intent при совпадении
`command_id + unit_id`. E5 подключает ACK barrier в FAT worker: каждый DO/AO step
ждёт terminal ACK, а timeout/negative ACK переводит step в `blocked` с
persisted evidence. Manual/sequence paths остаются открыты.
E6 делает execution state терминальным: timeout коммитится как отдельное состояние,
а поздний или повторный RESP не переписывает `acknowledged`, `negative_ack` или
`timeout`.
E7 отслеживает активные hardware leases FAT worker и освобождает их в cleanup даже
при исключении шага или rollback транзакции.

Все аппаратные отправители проходят один сервис `HardwareCommandAdmission`:

1. проверить device/channel capability и тип канала;
2. проверить актуальность allocation и workspace scope;
3. атомарно получить exclusive channel lease с `owner_kind`, `owner_id` и
   `fencing_epoch`;
4. сохранить command intent до публикации в MQTT stream;
5. отклонить expired/deadline-команду до физической отправки;
6. передать в protocol queue только принятый envelope.

Manual WS не должен сам выбирать correlation или обходить admission. Если
соединение/lease отсутствуют, оператор получает структурированный `rejected`,
а не optimistic success.

E8 подключает manual single-channel DO/AO к admission с обязательными
`workspace_id` и `channel_id`. Bulk/pair DO пока явно отклоняются с
`single_channel_admission_required`, пока не появится атомарный multi-channel
lease.

E9 подключает single-channel sequence steps к тому же lease, durable intent и ACK
barrier. E10 добавляет атомарный `acquire_many` для multi-channel операций.
E11 подключает `DO_PAIR` sequence к multi-channel lease, одному durable intent и
одному ACK barrier; `DO_BITMASK` остаётся отдельным случаем без resolved channel-set.
E12 подключает manual pair/all к `channel_ids` и тому же atomic multi-channel
lease; intent хранит primary channel и полный набор затронутых каналов.
E13 подключает sequence `DO_BITMASK` к resolved channel-set устройства и тому же
multi-channel lease, intent и ACK barrier.
E14 дополнительно проверяет workspace scope manual-команд через persisted
allocation до получения lease и публикации.
E15 публикует для manual WS `devices/command-result` с delivery `queued/rejected`,
чтобы оператор видел результат admission отдельно от device RESP.
E16 добавляет явную `verification_policy` для test-run: `off`, `optional` или
`required`; required блокирует шаги без IEC mapping/подписки, optional не блокирует.

## Состояния delivery и verdict

Это независимые поля:

| Поле | Значения |
| --- | --- |
| delivery | `created`, `queued`, `published`, `expired`, `rejected`, `publish_failed`, `recovery_required` |
| execution | `unknown`, `acknowledged`, `negative_ack`, `timeout` |
| FAT verdict | `pending`, `pass`, `fail`, `blocked`, `inconclusive` |

Только `acknowledged` с допустимым `command_id`, `unit_id`, `packet_id` и
`fencing_epoch` может перейти к execution success. `queued/published` не дают
PASS. Потеря ACK даёт `unknown` и блокирует автоматический retry до явной
политики idempotency.

При ошибке outbound publish intent получает `publish_failed`; для restore-команды
используется `recovery_required`, чтобы оператор/recovery worker видел риск
оставшегося физического состояния.

## IEC 61850 как опциональный контур

IEC 61850 не является глобально обязательным условием FAT. Для test plan/signal
должна быть явная политика:

- `off` — IEC 61850 не используется;
- `optional` — подписка и observation выполняются при доступности, но отсутствие
  или блокировка reserved report не блокирует командный тест;
- `required` — отсутствие mapping, подписки, свежего report или ожидаемого значения
  блокирует `PASS`.

RESP аппаратного модуля и IEC 61850 observation остаются разными видами evidence.
IEC report подтверждает наблюдение контроллером, но сам по себе не доказывает
физическое изменение выхода без отдельного feedback-сигнала.

Синхронное ожидание optional IEC report выполняется вне event loop FAT worker;
ошибка или timeout observation остаются явными `not_validated` evidence и не
превращаются в подтверждённый hardware PASS.

## RESP correlation

Inbound RESP сопоставляется по `(unit_id, packet_id)` с ожидающим command intent,
после чего дополнительно проверяются `command_id` и fencing epoch. Чужой,
дублированный или поздний RESP сохраняется как diagnostic event и не меняет
итог команды.

## API-изменение

Публичный WS-ответ для аппаратной команды предлагается расширить:

```json
{
  "event": "hardware_command_result",
  "command_id": "cmd-uuid",
  "delivery": "queued|rejected",
  "execution": "unknown",
  "reason": "channel_lease_busy"
}
```

Старые device state events не меняются. Сначала можно сохранить совместимость
ручных WS actions, направив их внутрь admission; переход frontend на ожидание
result event оформить отдельным подэтапом.

## Безопасность и lease

- lease должен быть workspace-независимым на уровне физического `channel_id`;
- expired owner не может командовать со старым `fencing_epoch`;
- release выполняется при terminal run/recovery;
- потеря lease во время run останавливает дальнейшие воздействия;
- restore/recovery получает отдельный owner kind и не обходится через manual path.

## Границы первого implementation slice

1. Добавить envelope/command intent и единый admission service.
2. Провести через него manual DO/AO и FAT DO/AO.
3. Добавить deterministic fake device adapter для ACK/timeout/duplicate/late RESP.
4. Не менять MQTT topic и binary frame в этом slice.
5. Sequence, AO policy и полноценный frontend result UI оставить отдельными
   подэтапами после стабилизации корреляции.

## Приёмка

- два workspace не получают один exclusive channel одновременно;
- queued/published без ACK не дают PASS;
- negative/late/duplicate/foreign ACK не меняет правильный command;
- expired command отклоняется до MQTT publish;
- старый fencing epoch не может воздействовать после lease loss;
- manual и FAT используют одну admission boundary.
