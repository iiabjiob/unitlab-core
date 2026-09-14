# Слайз E — единый admission и ACK для аппаратных команд

Статус: предложение на согласование, реализация не начата.
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

## Состояния delivery и verdict

Это независимые поля:

| Поле | Значения |
| --- | --- |
| delivery | `created`, `queued`, `published`, `expired`, `rejected` |
| execution | `unknown`, `acknowledged`, `negative_ack`, `timeout` |
| FAT verdict | `pending`, `pass`, `fail`, `blocked`, `inconclusive` |

Только `acknowledged` с допустимым `command_id`, `unit_id`, `packet_id` и
`fencing_epoch` может перейти к execution success. `queued/published` не дают
PASS. Потеря ACK даёт `unknown` и блокирует автоматический retry до явной
политики idempotency.

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

