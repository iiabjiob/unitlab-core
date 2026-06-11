#ifndef UNITLAB_IEC61850_IED_NATIVE_WIRE_CLIENT_H
#define UNITLAB_IEC61850_IED_NATIVE_WIRE_CLIENT_H

#include <stdint.h>

#include "model/model_loader.h"
#include "server/unitlab_mms_server_runtime.h"

typedef struct UnitLabNativeWireClientOptions {
    const char* initial_read_domain;
    const char* initial_read_item;
    uint32_t initial_read_invoke_id;
} UnitLabNativeWireClientOptions;

int unitlab_run_native_wire_client_with_options(
    const UnitLabIedServerConfig* config,
    const UnitLabNativeWireClientOptions* options,
    UnitLabIedModelLoadResult* result,
    UnitLabIedServerStopRequested stop_requested,
    void* stop_context);

int unitlab_run_native_wire_client(
    const UnitLabIedServerConfig* config,
    UnitLabIedModelLoadResult* result,
    UnitLabIedServerStopRequested stop_requested,
    void* stop_context);

#endif /* UNITLAB_IEC61850_IED_NATIVE_WIRE_CLIENT_H */
