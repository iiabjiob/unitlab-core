#include "model_loader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef UNITLAB_WITH_LIBIEC61850
#include <iec61850_dynamic_model.h>
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

#ifdef UNITLAB_WITH_LIBIEC61850
static int load_logical_devices_and_nodes(
    const UnitLabIedFixtureModel* fixture,
    const UnitLabIedModelPlan* plan,
    UnitLabIedModelLoadResult* result)
{
    IedModel* ied_model = IedModel_create(fixture->ied_name);
    if (ied_model == NULL) {
        set_result(result, "LIBIEC61850_IED_MODEL_CREATE_FAILED", "libIEC61850 failed to create the IED model root.");
        return 0;
    }
    IedModel_setIedNameForDynamicModel(ied_model, fixture->ied_name);

    LogicalDevice** logical_devices = (LogicalDevice**)calloc(plan->logical_device_count, sizeof(LogicalDevice*));
    if (logical_devices == NULL) {
        IedModel_destroy(ied_model);
        set_result(result, "OUT_OF_MEMORY", "Cannot allocate libIEC61850 logical device handles.");
        return 0;
    }

    for (size_t index = 0U; index < plan->logical_device_count; index++) {
        logical_devices[index] = LogicalDevice_create(plan->logical_devices[index].inst, ied_model);
        if (logical_devices[index] == NULL) {
            free(logical_devices);
            IedModel_destroy(ied_model);
            set_result(result, "LIBIEC61850_LOGICAL_DEVICE_CREATE_FAILED", "libIEC61850 failed to create a logical device.");
            return 0;
        }
    }

    for (size_t node_index = 0U; node_index < plan->logical_node_count; node_index++) {
        const UnitLabIedModelLogicalNode* node = &plan->logical_nodes[node_index];
        LogicalDevice* parent = NULL;
        for (size_t device_index = 0U; device_index < plan->logical_device_count; device_index++) {
            if (strcmp(plan->logical_devices[device_index].inst, node->logical_device_inst) == 0) {
                parent = logical_devices[device_index];
                break;
            }
        }
        if (parent == NULL || LogicalNode_create(node->name, parent) == NULL) {
            free(logical_devices);
            IedModel_destroy(ied_model);
            set_result(result, "LIBIEC61850_LOGICAL_NODE_CREATE_FAILED", "libIEC61850 failed to create a logical node.");
            return 0;
        }
    }

    free(logical_devices);
    IedModel_destroy(ied_model);
    set_result(
        result,
        "LIBIEC61850_DO_DA_LOADER_NOT_IMPLEMENTED",
        "libIEC61850 IED/LD/LN containers were created, but DO/DA/DataSet/RCB creation is not implemented in this slice.");
    return 0;
}
#endif

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
    (void)config;
    return load_logical_devices_and_nodes(fixture, plan, result);
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
