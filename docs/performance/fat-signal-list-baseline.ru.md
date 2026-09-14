# Baseline FAT signal-list

Статус: deterministic unit baseline; browser/Pi/soak baseline ещё не подтверждён.

## Повторяемый прогон

```bash
npm --prefix frontend run test -- --run src/pages/signals/utils/signalListPerformanceHarness.test.ts
```

Harness фиксирует сценарий на 20 000 строк и burst из 5 000 allocation/runtime
patches. Он проверяет, что обычные изменения не вызывают full reload, grid work
отложен до flush, patch payload ограничен изменившимися строками, а selection и
channel-owner lookup проходят по полному набору строк.

## Что этот baseline не доказывает

- реальную скорость browser rendering, scrolling, editing и selection;
- отсутствие long tasks, frame drops и роста памяти на Pi за смену;
- MQTT/WS throughput, hardware command→ACK latency и IEC 61850 latency;
- backend query count при пакетной подготовке test-run.

Для release-приёмки нужны browser trace на целевом Pi, offline/reconnect
сценарий и soak с зафиксированными частотами телеметрии и bursts.
