#ifndef UNITLAB_IEC61850_LIBIEC61850_MODEL_BUILDER_H
#define UNITLAB_IEC61850_LIBIEC61850_MODEL_BUILDER_H

#include "model_loader.h"

#ifdef UNITLAB_WITH_LIBIEC61850
int unitlab_validate_libiec61850_dynamic_model(
    const UnitLabIedFixtureModel* fixture,
    const UnitLabIedModelPlan* plan,
    UnitLabIedModelLoadResult* result);
#endif

#endif
