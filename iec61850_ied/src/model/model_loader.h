#ifndef UNITLAB_IEC61850_IED_MODEL_LOADER_H
#define UNITLAB_IEC61850_IED_MODEL_LOADER_H

#include <stddef.h>

#include "fixture/fixture_parser.h"
#include "model/model_plan.h"
#include "server/server_runtime.h"

int unitlab_load_ied_model(
    const UnitLabIedFixtureModel* fixture,
    const UnitLabIedModelPlan* plan,
    const UnitLabIedServerConfig* config,
    UnitLabIedModelLoadResult* result);

int unitlab_run_ied_server(
    const UnitLabIedFixtureModel* fixture,
    const UnitLabIedModelPlan* plan,
    const UnitLabIedServerConfig* config,
    UnitLabIedServerStopRequested stop_requested,
    void* stop_context,
    UnitLabIedModelLoadResult* result);

#endif
