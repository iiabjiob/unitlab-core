#include "libiec61850_model_builder.h"

#ifdef UNITLAB_WITH_LIBIEC61850

#include <iec61850_dynamic_model.h>
#include <iec61850_server.h>
#include <hal_thread.h>
#include <mms_value.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct UnitLabLibIedModelHandles {
    IedModel* ied_model;
    LogicalDevice** logical_devices;
    LogicalNode** logical_nodes;
} UnitLabLibIedModelHandles;

static void set_result(UnitLabIedModelLoadResult* result, const char* code, const char* message)
{
    if (result == NULL) {
        return;
    }
    result->loaded = 0;
    snprintf(result->code, sizeof(result->code), "%s", code);
    snprintf(result->message, sizeof(result->message), "%s", message);
}

static void destroy_libiec61850_handles(UnitLabLibIedModelHandles* handles)
{
    if (handles == NULL) {
        return;
    }
    free(handles->logical_nodes);
    free(handles->logical_devices);
    if (handles->ied_model != NULL) {
        IedModel_destroy(handles->ied_model);
    }
    memset(handles, 0, sizeof(*handles));
}

static LogicalDevice* find_logical_device(
    const UnitLabIedModelPlan* plan,
    UnitLabLibIedModelHandles* handles,
    const char* logical_device_inst)
{
    for (size_t index = 0U; index < plan->logical_device_count; index++) {
        if (strcmp(plan->logical_devices[index].inst, logical_device_inst) == 0) {
            return handles->logical_devices[index];
        }
    }
    return NULL;
}

static LogicalNode* find_logical_node(
    const UnitLabIedModelPlan* plan,
    UnitLabLibIedModelHandles* handles,
    const char* logical_device_inst,
    const char* logical_node_name)
{
    for (size_t index = 0U; index < plan->logical_node_count; index++) {
        if (
            strcmp(plan->logical_nodes[index].logical_device_inst, logical_device_inst) == 0
            && strcmp(plan->logical_nodes[index].name, logical_node_name) == 0
        ) {
            return handles->logical_nodes[index];
        }
    }
    return NULL;
}

static ModelNode* first_child(ModelNode* parent)
{
    if (parent == NULL) {
        return NULL;
    }
    if (parent->modelType == LogicalNodeModelType) {
        return ((LogicalNode*)parent)->firstChild;
    }
    if (parent->modelType == DataObjectModelType) {
        return ((DataObject*)parent)->firstChild;
    }
    if (parent->modelType == DataAttributeModelType) {
        return ((DataAttribute*)parent)->firstChild;
    }
    return NULL;
}

static DataObject* find_data_object(LogicalNode* parent, const char* name)
{
    for (ModelNode* child = first_child((ModelNode*)parent); child != NULL; child = child->sibling) {
        if (child->modelType == DataObjectModelType && strcmp(child->name, name) == 0) {
            return (DataObject*)child;
        }
    }
    return NULL;
}

static DataAttribute* find_data_attribute(ModelNode* parent, const char* name, FunctionalConstraint fc)
{
    for (ModelNode* child = first_child(parent); child != NULL; child = child->sibling) {
        if (child->modelType != DataAttributeModelType || strcmp(child->name, name) != 0) {
            continue;
        }
        DataAttribute* attribute = (DataAttribute*)child;
        if (DataAttribute_getFC(attribute) == fc) {
            return attribute;
        }
    }
    return NULL;
}

static int parse_int32(const char* source, int32_t* value)
{
    char* end = NULL;
    long parsed = strtol(source, &end, 10);
    if (source == end || end == NULL || *end != '\0' || parsed < INT32_MIN || parsed > INT32_MAX) {
        return 0;
    }
    *value = (int32_t)parsed;
    return 1;
}

static int parse_real32(const char* source, float* value)
{
    char* end = NULL;
    double parsed = strtod(source, &end);
    if (source == end || end == NULL || *end != '\0') {
        return 0;
    }
    *value = (float)parsed;
    return 1;
}

static int data_attribute_type_for_value(
    const UnitLabIedModelSignal* signal,
    DataAttributeType* type,
    UnitLabIedModelLoadResult* result)
{
    switch (signal->initial_value_kind) {
        case UNITLAB_IED_FIXTURE_VALUE_BOOLEAN:
            *type = IEC61850_BOOLEAN;
            return 1;
        case UNITLAB_IED_FIXTURE_VALUE_INTEGER:
            *type = IEC61850_INT32;
            return 1;
        case UNITLAB_IED_FIXTURE_VALUE_REAL:
            *type = IEC61850_FLOAT32;
            return 1;
        case UNITLAB_IED_FIXTURE_VALUE_STRING:
            *type = IEC61850_VISIBLE_STRING_255;
            return 1;
        case UNITLAB_IED_FIXTURE_VALUE_NULL:
            set_result(
                result,
                "LIBIEC61850_NULL_INITIAL_VALUE_UNSUPPORTED",
                "A DataSet member has null initialValue; the simulator fixture needs a typed value before MMS can expose it.");
            return 0;
        case UNITLAB_IED_FIXTURE_VALUE_UNKNOWN:
        default:
            set_result(
                result,
                "LIBIEC61850_INITIAL_VALUE_KIND_UNSUPPORTED",
                "A DataSet member has an unsupported initialValue kind.");
            return 0;
    }
}

static MmsValue* create_initial_mms_value(
    const UnitLabIedModelSignal* signal,
    UnitLabIedModelLoadResult* result)
{
    switch (signal->initial_value_kind) {
        case UNITLAB_IED_FIXTURE_VALUE_BOOLEAN:
            if (strcmp(signal->initial_value, "true") == 0) {
                return MmsValue_newBoolean(true);
            }
            if (strcmp(signal->initial_value, "false") == 0) {
                return MmsValue_newBoolean(false);
            }
            set_result(result, "LIBIEC61850_BOOLEAN_VALUE_INVALID", "A boolean initialValue is not true or false.");
            return NULL;
        case UNITLAB_IED_FIXTURE_VALUE_INTEGER: {
            int32_t value = 0;
            if (!parse_int32(signal->initial_value, &value)) {
                set_result(result, "LIBIEC61850_INTEGER_VALUE_INVALID", "An integer initialValue is outside int32 range.");
                return NULL;
            }
            return MmsValue_newIntegerFromInt32(value);
        }
        case UNITLAB_IED_FIXTURE_VALUE_REAL: {
            float value = 0.0F;
            if (!parse_real32(signal->initial_value, &value)) {
                set_result(result, "LIBIEC61850_REAL_VALUE_INVALID", "A real initialValue cannot be parsed.");
                return NULL;
            }
            return MmsValue_newFloat(value);
        }
        case UNITLAB_IED_FIXTURE_VALUE_STRING:
            return MmsValue_newVisibleString(signal->initial_value);
        case UNITLAB_IED_FIXTURE_VALUE_NULL:
        case UNITLAB_IED_FIXTURE_VALUE_UNKNOWN:
        default:
            set_result(result, "LIBIEC61850_INITIAL_VALUE_KIND_UNSUPPORTED", "A DataSet member has an unsupported initialValue kind.");
            return NULL;
    }
}

static int create_data_attribute_path(
    DataObject* data_object,
    const UnitLabIedModelSignal* signal,
    FunctionalConstraint fc,
    UnitLabIedModelLoadResult* result)
{
    if (signal->data_attribute_path[0] == '\0') {
        return 1;
    }

    DataAttributeType leaf_type = IEC61850_UNKNOWN_TYPE;
    if (!data_attribute_type_for_value(signal, &leaf_type, result)) {
        return 0;
    }

    char path[sizeof(signal->data_attribute_path)];
    snprintf(path, sizeof(path), "%s", signal->data_attribute_path);
    ModelNode* parent = (ModelNode*)data_object;
    char* cursor = path;
    while (cursor != NULL && *cursor != '\0') {
        char* dot = strchr(cursor, '.');
        int is_leaf = dot == NULL;
        if (dot != NULL) {
            *dot = '\0';
        }

        DataAttributeType attribute_type = is_leaf ? leaf_type : IEC61850_CONSTRUCTED;
        uint8_t trigger_options = is_leaf ? TRG_OPT_DATA_CHANGED : 0U;
        DataAttribute* attribute = find_data_attribute(parent, cursor, fc);
        if (attribute == NULL) {
            attribute = DataAttribute_create(cursor, parent, attribute_type, fc, trigger_options, 0, 0);
        }
        if (attribute == NULL) {
            set_result(result, "LIBIEC61850_DATA_ATTRIBUTE_CREATE_FAILED", "libIEC61850 failed to create a data attribute.");
            return 0;
        }

        if (is_leaf) {
            MmsValue* value = create_initial_mms_value(signal, result);
            if (value == NULL) {
                return 0;
            }
            DataAttribute_setValue(attribute, value);
            MmsValue_delete(value);
        }

        parent = (ModelNode*)attribute;
        cursor = dot == NULL ? NULL : dot + 1;
    }

    return 1;
}

static int create_signal_model(
    const UnitLabIedModelPlan* plan,
    UnitLabLibIedModelHandles* handles,
    const UnitLabIedModelSignal* signal,
    UnitLabIedModelLoadResult* result)
{
    LogicalNode* logical_node = find_logical_node(plan, handles, signal->logical_device_inst, signal->logical_node_name);
    if (logical_node == NULL) {
        set_result(result, "LIBIEC61850_SIGNAL_NODE_NOT_FOUND", "A DataSet member references an LN that was not created.");
        return 0;
    }

    DataObject* data_object = find_data_object(logical_node, signal->data_object_name);
    if (data_object == NULL) {
        data_object = DataObject_create(signal->data_object_name, (ModelNode*)logical_node, 0);
    }
    if (data_object == NULL) {
        set_result(result, "LIBIEC61850_DATA_OBJECT_CREATE_FAILED", "libIEC61850 failed to create a data object.");
        return 0;
    }

    FunctionalConstraint fc = FunctionalConstraint_fromString(signal->fc);
    if (fc == IEC61850_FC_NONE) {
        set_result(result, "LIBIEC61850_FUNCTIONAL_CONSTRAINT_UNSUPPORTED", "A DataSet member has an unsupported functional constraint.");
        return 0;
    }

    return create_data_attribute_path(data_object, signal, fc, result);
}

static int create_data_sets(
    const UnitLabIedModelPlan* plan,
    UnitLabLibIedModelHandles* handles,
    DataSet*** data_sets_out,
    UnitLabIedModelLoadResult* result)
{
    DataSet** data_sets = (DataSet**)calloc(plan->data_set_count, sizeof(DataSet*));
    if (data_sets == NULL) {
        set_result(result, "OUT_OF_MEMORY", "Cannot allocate libIEC61850 DataSet handles.");
        return 0;
    }

    for (size_t index = 0U; index < plan->data_set_count; index++) {
        const UnitLabIedModelDataSet* data_set = &plan->data_sets[index];
        LogicalNode* parent = find_logical_node(plan, handles, data_set->logical_device_inst, data_set->logical_node_name);
        if (parent == NULL) {
            free(data_sets);
            set_result(result, "LIBIEC61850_DATASET_NODE_NOT_FOUND", "A DataSet references an LN that was not created.");
            return 0;
        }

        data_sets[index] = DataSet_create(data_set->name, parent);
        if (data_sets[index] == NULL) {
            free(data_sets);
            set_result(result, "LIBIEC61850_DATASET_CREATE_FAILED", "libIEC61850 failed to create a DataSet.");
            return 0;
        }

        for (size_t member = 0U; member < data_set->member_count; member++) {
            size_t signal_index = data_set->first_signal_index + member;
            const UnitLabIedModelSignal* signal = &plan->signals[signal_index];
            const char* component = signal->data_set_entry_component_known ? signal->data_set_entry_component : NULL;
            if (DataSetEntry_create(data_sets[index], signal->data_set_entry_variable, -1, component) == NULL) {
                free(data_sets);
                set_result(result, "LIBIEC61850_DATASET_ENTRY_CREATE_FAILED", "libIEC61850 failed to create a DataSetEntry.");
                return 0;
            }
        }
    }

    *data_sets_out = data_sets;
    return 1;
}

static int create_report_controls(
    const UnitLabIedModelPlan* plan,
    UnitLabLibIedModelHandles* handles,
    UnitLabIedModelLoadResult* result)
{
    for (size_t index = 0U; index < plan->report_count; index++) {
        const UnitLabIedModelReportControl* report = &plan->reports[index];
        const UnitLabIedModelDataSet* data_set = &plan->data_sets[report->data_set_index];
        LogicalNode* parent = find_logical_node(plan, handles, report->logical_device_inst, report->logical_node_name);
        if (parent == NULL) {
            set_result(result, "LIBIEC61850_REPORT_NODE_NOT_FOUND", "A ReportControl references an LN that was not created.");
            return 0;
        }

        uint32_t conf_rev = report->conf_rev_known ? report->conf_rev : 1U;
        uint32_t buffer_time = report->buffer_time_ms_known ? report->buffer_time_ms : 0U;
        uint32_t integrity_period = report->integrity_period_ms_known ? report->integrity_period_ms : 0U;
        const char* rpt_id = report->rpt_id[0] == '\0' ? NULL : report->rpt_id;

        if (
            ReportControlBlock_create(
                report->name,
                parent,
                rpt_id,
                report->is_buffered ? true : false,
                data_set->name,
                conf_rev,
                report->trigger_options_mask,
                report->optional_fields_mask,
                buffer_time,
                integrity_period) == NULL
        ) {
            set_result(result, "LIBIEC61850_REPORT_CREATE_FAILED", "libIEC61850 failed to create a ReportControlBlock.");
            return 0;
        }
    }
    return 1;
}

static int create_libiec61850_model(
    const UnitLabIedFixtureModel* fixture,
    const UnitLabIedModelPlan* plan,
    UnitLabLibIedModelHandles* handles,
    UnitLabIedModelLoadResult* result)
{
    handles->ied_model = IedModel_create(fixture->ied_name);
    if (handles->ied_model == NULL) {
        set_result(result, "LIBIEC61850_IED_MODEL_CREATE_FAILED", "libIEC61850 failed to create the IED model root.");
        return 0;
    }
    IedModel_setIedNameForDynamicModel(handles->ied_model, fixture->ied_name);

    handles->logical_devices = (LogicalDevice**)calloc(plan->logical_device_count, sizeof(LogicalDevice*));
    handles->logical_nodes = (LogicalNode**)calloc(plan->logical_node_count, sizeof(LogicalNode*));
    if (handles->logical_devices == NULL || handles->logical_nodes == NULL) {
        set_result(result, "OUT_OF_MEMORY", "Cannot allocate libIEC61850 logical device handles.");
        return 0;
    }

    for (size_t index = 0U; index < plan->logical_device_count; index++) {
        handles->logical_devices[index] = LogicalDevice_create(plan->logical_devices[index].inst, handles->ied_model);
        if (handles->logical_devices[index] == NULL) {
            set_result(result, "LIBIEC61850_LOGICAL_DEVICE_CREATE_FAILED", "libIEC61850 failed to create a logical device.");
            return 0;
        }
    }

    for (size_t node_index = 0U; node_index < plan->logical_node_count; node_index++) {
        const UnitLabIedModelLogicalNode* node = &plan->logical_nodes[node_index];
        LogicalDevice* parent = find_logical_device(plan, handles, node->logical_device_inst);
        if (parent == NULL) {
            set_result(result, "LIBIEC61850_LOGICAL_DEVICE_NOT_FOUND", "A logical node references an LD that was not created.");
            return 0;
        }
        handles->logical_nodes[node_index] = LogicalNode_create(node->name, parent);
        if (handles->logical_nodes[node_index] == NULL) {
            set_result(result, "LIBIEC61850_LOGICAL_NODE_CREATE_FAILED", "libIEC61850 failed to create a logical node.");
            return 0;
        }
    }

    for (size_t signal_index = 0U; signal_index < plan->signal_count; signal_index++) {
        if (!create_signal_model(plan, handles, &plan->signals[signal_index], result)) {
            return 0;
        }
    }

    DataSet** data_sets = NULL;
    if (!create_data_sets(plan, handles, &data_sets, result)) {
        return 0;
    }
    free(data_sets);

    return create_report_controls(plan, handles, result);
}

int unitlab_validate_libiec61850_dynamic_model(
    const UnitLabIedFixtureModel* fixture,
    const UnitLabIedModelPlan* plan,
    UnitLabIedModelLoadResult* result)
{
    UnitLabLibIedModelHandles handles = {0};
    int created = create_libiec61850_model(fixture, plan, &handles, result);
    destroy_libiec61850_handles(&handles);
    return created;
}

int unitlab_run_libiec61850_server(
    const UnitLabIedFixtureModel* fixture,
    const UnitLabIedModelPlan* plan,
    const UnitLabIedServerConfig* config,
    UnitLabIedServerStopRequested stop_requested,
    void* stop_context,
    UnitLabIedModelLoadResult* result)
{
    UnitLabLibIedModelHandles handles = {0};
    IedServer server = NULL;
    if (!create_libiec61850_model(fixture, plan, &handles, result)) {
        destroy_libiec61850_handles(&handles);
        return 0;
    }

    server = IedServer_create(handles.ied_model);
    if (server == NULL) {
        destroy_libiec61850_handles(&handles);
        set_result(result, "LIBIEC61850_SERVER_CREATE_FAILED", "libIEC61850 failed to create the IED server.");
        return 0;
    }

    IedServer_setLocalIpAddress(server, config->bind_address);
    IedServer_start(server, config->port);
    if (!IedServer_isRunning(server)) {
        IedServer_destroy(server);
        destroy_libiec61850_handles(&handles);
        set_result(result, "LIBIEC61850_SERVER_START_FAILED", "libIEC61850 failed to start the IED server on the requested endpoint.");
        return 0;
    }

    result->loaded = 1;
    snprintf(result->code, sizeof(result->code), "%s", "LIBIEC61850_SERVER_RUNNING");
    snprintf(result->message, sizeof(result->message), "%s", "libIEC61850 IED server is running.");

    while (stop_requested == NULL || !stop_requested(stop_context)) {
        Thread_sleep(100);
    }

    IedServer_stop(server);
    IedServer_destroy(server);
    destroy_libiec61850_handles(&handles);
    result->loaded = 0;
    snprintf(result->code, sizeof(result->code), "%s", "LIBIEC61850_SERVER_STOPPED");
    snprintf(result->message, sizeof(result->message), "%s", "libIEC61850 IED server stopped cleanly.");
    return 1;
}

#endif
