#ifndef UNITLAB_IEC61850_IED_MODEL_LOADER_H
#define UNITLAB_IEC61850_IED_MODEL_LOADER_H

#include <stddef.h>

#include "fixture_parser.h"
#include "model_plan.h"

typedef struct UnitLabIedServerConfig {
    const char* bind_address;
    int port;
} UnitLabIedServerConfig;

typedef struct UnitLabIedModelLoadResult {
    int loaded;
    char code[64];
    char message[256];
} UnitLabIedModelLoadResult;

int unitlab_load_ied_model(
    const UnitLabIedFixtureModel* fixture,
    const UnitLabIedModelPlan* plan,
    const UnitLabIedServerConfig* config,
    UnitLabIedModelLoadResult* result);

#endif
