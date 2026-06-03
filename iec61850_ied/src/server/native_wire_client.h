#ifndef UNITLAB_IEC61850_IED_NATIVE_WIRE_CLIENT_H
#define UNITLAB_IEC61850_IED_NATIVE_WIRE_CLIENT_H

#include "model/model_loader.h"
#include "server/unitlab_mms_server_runtime.h"

int unitlab_run_native_wire_client(
    const UnitLabIedServerConfig* config,
    UnitLabIedModelLoadResult* result,
    UnitLabIedServerStopRequested stop_requested,
    void* stop_context);

#endif /* UNITLAB_IEC61850_IED_NATIVE_WIRE_CLIENT_H */
