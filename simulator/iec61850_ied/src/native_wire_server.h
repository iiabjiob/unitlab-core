#ifndef UNITLAB_IEC61850_IED_NATIVE_WIRE_SERVER_H
#define UNITLAB_IEC61850_IED_NATIVE_WIRE_SERVER_H

#include "model_loader.h"
#include "unitlab_mms_server_runtime.h"

int unitlab_run_native_wire_server(
    UnitLabMmsServerRuntime* server_runtime,
    const UnitLabIedServerConfig* config,
    UnitLabIedModelLoadResult* result,
    UnitLabIedServerStopRequested stop_requested,
    void* stop_context);

#endif /* UNITLAB_IEC61850_IED_NATIVE_WIRE_SERVER_H */
