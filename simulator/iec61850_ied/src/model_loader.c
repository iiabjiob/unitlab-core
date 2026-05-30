#include "model_loader.h"

#include <stdio.h>
#include <string.h>

#ifdef UNITLAB_WITH_LIBIEC61850
#include <iec61850_server.h>
#endif

static void set_result(UnitLabIedModelLoadResult* result, const char* code, const char* message)
{
    if (result == NULL) {
        return;
    }
    result->loaded = 0;
    snprintf(result->code, sizeof(result->code), "%s", code);
    snprintf(result->message, sizeof(result->message), "%s", message);
}

static int validate_loader_inputs(
    const UnitLabIedFixtureModel* fixture,
    const UnitLabIedModelPlan* plan,
    const UnitLabIedServerConfig* config,
    UnitLabIedModelLoadResult* result)
{
    if (fixture == NULL || plan == NULL || config == NULL || result == NULL) {
        set_result(result, "MODEL_LOADER_INVALID_ARGUMENT", "Fixture, model plan, server config, and result are required.");
        return 0;
    }
    if (fixture->ied_name[0] == '\0' || fixture->access_point_name[0] == '\0') {
        set_result(result, "MODEL_LOADER_INVALID_FIXTURE", "Fixture requires IED name and access point name.");
        return 0;
    }
    if (
        plan->logical_device_count == 0U
        || plan->logical_node_count == 0U
        || plan->data_set_count == 0U
        || plan->report_count == 0U
        || plan->signal_count == 0U
    ) {
        set_result(result, "MODEL_LOADER_EMPTY_PLAN", "Model plan requires at least one LD, LN, DataSet, signal, and ReportControl.");
        return 0;
    }
    if (config->bind_address == NULL || config->bind_address[0] == '\0') {
        set_result(result, "MODEL_LOADER_INVALID_BIND", "Bind address is required.");
        return 0;
    }
    if (config->port <= 0 || config->port > 65535) {
        set_result(result, "MODEL_LOADER_INVALID_PORT", "Port must be in range 1..65535.");
        return 0;
    }
    return 1;
}

int unitlab_load_ied_model(
    const UnitLabIedFixtureModel* fixture,
    const UnitLabIedModelPlan* plan,
    const UnitLabIedServerConfig* config,
    UnitLabIedModelLoadResult* result)
{
    if (result != NULL) {
        memset(result, 0, sizeof(*result));
    }
    if (!validate_loader_inputs(fixture, plan, config, result)) {
        return 0;
    }

#ifdef UNITLAB_WITH_LIBIEC61850
    (void)fixture;
    (void)plan;
    (void)config;
    set_result(
        result,
        "LIBIEC61850_MODEL_LOADER_NOT_IMPLEMENTED",
        "libIEC61850 is linked, but UnitLab IED model creation is not implemented in this slice.");
    return 0;
#else
    (void)fixture;
    (void)plan;
    (void)config;
    set_result(
        result,
        "LIBIEC61850_NOT_LINKED",
        "libIEC61850 is not linked; build with UNITLAB_IEC61850_SIM_WITH_LIBIEC61850=ON before starting the MMS server.");
    return 0;
#endif
}
