#ifndef UNITLAB_IEC61850_IED_CLIENT_PROBE_H
#define UNITLAB_IEC61850_IED_CLIENT_PROBE_H

#include "fixture_parser.h"
#include "model_plan.h"
#include "server_runtime.h"

int unitlab_probe_ied_server_metadata(
    const UnitLabIedFixtureModel* fixture,
    const UnitLabIedModelPlan* plan,
    const UnitLabIedServerConfig* config,
    UnitLabIedModelLoadResult* result);

int unitlab_probe_ied_server_gi(
    const UnitLabIedFixtureModel* fixture,
    const UnitLabIedModelPlan* plan,
    const UnitLabIedServerConfig* config,
    const char* report_key,
    UnitLabIedModelLoadResult* result);

#endif
