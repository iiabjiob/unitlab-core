#ifndef UNITLAB_IEC61850_LIBIEC61850_MODEL_BUILDER_H
#define UNITLAB_IEC61850_LIBIEC61850_MODEL_BUILDER_H

#include "fixture/fixture_parser.h"
#include "model/model_plan.h"
#include "server/server_runtime.h"

#ifdef UNITLAB_WITH_LIBIEC61850
int unitlab_validate_libiec61850_dynamic_model(
    const UnitLabIedFixtureModel* fixture,
    const UnitLabIedModelPlan* plan,
    UnitLabIedModelLoadResult* result);

int unitlab_run_libiec61850_server(
    const UnitLabIedFixtureModel* fixture,
    const UnitLabIedModelPlan* plan,
    const UnitLabIedServerConfig* config,
    UnitLabIedServerStopRequested stop_requested,
    void* stop_context,
    UnitLabIedModelLoadResult* result);
#endif

#endif
