# Wire Test Targets

This project splits wire-layer tests by protocol layer so failures are easier to localize.

## Build all wire tests

From the repository root, build the focused wire targets explicitly:

```sh
cmake --build build --target \
  unitlab-iec61850-wire-ber-test \
  unitlab-iec61850-wire-tpkt-test \
  unitlab-iec61850-wire-cotp-test \
  unitlab-iec61850-wire-transport-frame-test \
  unitlab-iec61850-wire-session-test \
  unitlab-iec61850-wire-presentation-test \
  unitlab-iec61850-wire-acse-test \
  unitlab-iec61850-wire-mms-pdu-test \
  unitlab-iec61850-wire-association-frame-test \
  unitlab-iec61850-wire-builder-test \
  unitlab-iec61850-wire-stack-ladder-test
```

## Run all wire tests

```sh
ctest --test-dir build --output-on-failure -R '^unitlab-iec61850-wire-(ber|tpkt|cotp|transport-frame|session|presentation|acse|mms-pdu|association-frame|builder|stack-ladder)$'
```

## Run one layer

Run a single focused layer target when you want a narrow failure boundary. For example, ACSE: 

```sh
ctest --test-dir build --output-on-failure -R '^unitlab-iec61850-wire-acse$'
```

You can substitute any other layer target such as `wire-ber`, `wire-tpkt`, or `wire-presentation`.

## Layer vs ladder

- Layer tests verify one protocol boundary in isolation.
- Stack-ladder tests verify that adjacent layers compose correctly end to end.

