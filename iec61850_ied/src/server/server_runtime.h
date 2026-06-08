#ifndef UNITLAB_IEC61850_IED_SERVER_RUNTIME_H
#define UNITLAB_IEC61850_IED_SERVER_RUNTIME_H

typedef struct UnitLabIedServerConfig {
    const char* bind_address;
    int port;
    int control_port;
    int native_test_report_tick_ms;
} UnitLabIedServerConfig;

typedef struct UnitLabIedModelLoadResult {
    int loaded;
    char code[64];
    char message[256];
} UnitLabIedModelLoadResult;

typedef int (*UnitLabIedServerStopRequested)(void* context);

#endif
