#include "model_plan.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void set_error(char* error, size_t error_size, const char* format, ...)
{
    if (error == NULL || error_size == 0U) {
        return;
    }
    va_list args;
    va_start(args, format);
    vsnprintf(error, error_size, format, args);
    va_end(args);
}

static int copy_string(char* destination, size_t destination_size, const char* source)
{
    if (destination == NULL || destination_size == 0U || source == NULL) {
        return 0;
    }
    size_t source_length = strlen(source);
    if (source_length >= destination_size) {
        return 0;
    }
    memcpy(destination, source, source_length + 1U);
    return 1;
}

static int parse_signal_logical_node(
    const char* reference,
    char* logical_device_inst,
    size_t logical_device_size,
    char* logical_node_name,
    size_t logical_node_size)
{
    const char* separator = strchr(reference, '/');
    if (separator == NULL || separator == reference) {
        return 0;
    }

    const char* node_start = separator + 1;
    const char* dot = strchr(node_start, '.');
    const char* slash = strchr(node_start, '/');
    const char* node_end = dot;
    if (node_end == NULL || (slash != NULL && slash < node_end)) {
        node_end = slash;
    }
    if (node_end == NULL || node_end == node_start) {
        return 0;
    }

    size_t ld_length = (size_t)(separator - reference);
    size_t node_length = (size_t)(node_end - node_start);
    if (ld_length >= logical_device_size || node_length >= logical_node_size) {
        return 0;
    }

    memcpy(logical_device_inst, reference, ld_length);
    logical_device_inst[ld_length] = '\0';
    memcpy(logical_node_name, node_start, node_length);
    logical_node_name[node_length] = '\0';
    return 1;
}

static int has_logical_device(const UnitLabIedModelPlan* plan, const char* logical_device_inst)
{
    for (size_t index = 0U; index < plan->logical_device_count; index++) {
        if (strcmp(plan->logical_devices[index].inst, logical_device_inst) == 0) {
            return 1;
        }
    }
    return 0;
}

static int add_logical_device(UnitLabIedModelPlan* plan, const char* logical_device_inst)
{
    if (has_logical_device(plan, logical_device_inst)) {
        return 1;
    }
    if (!copy_string(plan->logical_devices[plan->logical_device_count].inst, sizeof(plan->logical_devices[plan->logical_device_count].inst), logical_device_inst)) {
        return 0;
    }
    plan->logical_device_count++;
    return 1;
}

static int has_logical_node(const UnitLabIedModelPlan* plan, const char* logical_device_inst, const char* logical_node_name)
{
    for (size_t index = 0U; index < plan->logical_node_count; index++) {
        const UnitLabIedModelLogicalNode* node = &plan->logical_nodes[index];
        if (
            strcmp(node->logical_device_inst, logical_device_inst) == 0
            && strcmp(node->name, logical_node_name) == 0
        ) {
            return 1;
        }
    }
    return 0;
}

static int add_logical_node(UnitLabIedModelPlan* plan, const char* logical_device_inst, const char* logical_node_name)
{
    if (has_logical_node(plan, logical_device_inst, logical_node_name)) {
        return 1;
    }
    UnitLabIedModelLogicalNode* node = &plan->logical_nodes[plan->logical_node_count];
    if (!copy_string(node->logical_device_inst, sizeof(node->logical_device_inst), logical_device_inst)) {
        return 0;
    }
    if (!copy_string(node->name, sizeof(node->name), logical_node_name)) {
        return 0;
    }
    plan->logical_node_count++;
    return 1;
}

static int add_node_reference(UnitLabIedModelPlan* plan, const char* logical_device_inst, const char* logical_node_name)
{
    return add_logical_device(plan, logical_device_inst)
        && add_logical_node(plan, logical_device_inst, logical_node_name);
}

static int find_data_set_index(const UnitLabIedFixtureModel* fixture, const char* data_set_ref, size_t* data_set_index)
{
    for (size_t index = 0U; index < fixture->data_set_count; index++) {
        if (strcmp(fixture->data_sets[index].reference, data_set_ref) == 0) {
            *data_set_index = index;
            return 1;
        }
    }
    return 0;
}

static int allocate_plan(const UnitLabIedFixtureModel* fixture, UnitLabIedModelPlan* plan, char* error, size_t error_size)
{
    size_t max_logical_devices = fixture->signal_count + fixture->report_count;
    size_t max_logical_nodes = fixture->signal_count + fixture->report_count;
    if (max_logical_devices == 0U || max_logical_nodes == 0U) {
        set_error(error, error_size, "MODEL_PLAN_EMPTY: fixture has no signals or reports.");
        return 0;
    }

    plan->logical_devices = (UnitLabIedModelLogicalDevice*)calloc(max_logical_devices, sizeof(UnitLabIedModelLogicalDevice));
    plan->logical_nodes = (UnitLabIedModelLogicalNode*)calloc(max_logical_nodes, sizeof(UnitLabIedModelLogicalNode));
    plan->data_sets = (UnitLabIedModelDataSet*)calloc(fixture->data_set_count, sizeof(UnitLabIedModelDataSet));
    plan->reports = (UnitLabIedModelReportControl*)calloc(fixture->report_count, sizeof(UnitLabIedModelReportControl));
    if (
        plan->logical_devices == NULL
        || plan->logical_nodes == NULL
        || plan->data_sets == NULL
        || plan->reports == NULL
    ) {
        set_error(error, error_size, "OUT_OF_MEMORY: cannot allocate IEC 61850 model plan.");
        return 0;
    }
    return 1;
}

int unitlab_build_ied_model_plan(
    const UnitLabIedFixtureModel* fixture,
    UnitLabIedModelPlan* plan,
    char* error,
    size_t error_size)
{
    if (fixture == NULL || plan == NULL) {
        set_error(error, error_size, "INVALID_ARGUMENT: fixture and model plan are required.");
        return 0;
    }

    memset(plan, 0, sizeof(*plan));
    if (!allocate_plan(fixture, plan, error, error_size)) {
        unitlab_free_ied_model_plan(plan);
        return 0;
    }

    for (size_t data_set_index = 0U; data_set_index < fixture->data_set_count; data_set_index++) {
        const UnitLabIedFixtureDataSet* data_set = &fixture->data_sets[data_set_index];
        UnitLabIedModelDataSet* model_data_set = &plan->data_sets[data_set_index];
        if (!copy_string(model_data_set->reference, sizeof(model_data_set->reference), data_set->reference)) {
            set_error(error, error_size, "MODEL_PLAN_DATASET_REFERENCE_TOO_LONG: %s", data_set->reference);
            unitlab_free_ied_model_plan(plan);
            return 0;
        }
        model_data_set->member_count = data_set->signal_count;
        plan->data_set_count++;

        for (size_t signal_index = 0U; signal_index < data_set->signal_count; signal_index++) {
            char logical_device_inst[128];
            char logical_node_name[128];
            if (!parse_signal_logical_node(
                    data_set->signals[signal_index].reference,
                    logical_device_inst,
                    sizeof(logical_device_inst),
                    logical_node_name,
                    sizeof(logical_node_name))) {
                set_error(error, error_size, "MODEL_PLAN_SIGNAL_REFERENCE_INVALID: %s", data_set->signals[signal_index].reference);
                unitlab_free_ied_model_plan(plan);
                return 0;
            }
            if (!add_node_reference(plan, logical_device_inst, logical_node_name)) {
                set_error(error, error_size, "MODEL_PLAN_NODE_REFERENCE_TOO_LONG: %s/%s", logical_device_inst, logical_node_name);
                unitlab_free_ied_model_plan(plan);
                return 0;
            }
        }
    }

    for (size_t report_index = 0U; report_index < fixture->report_count; report_index++) {
        const UnitLabIedFixtureReport* report = &fixture->reports[report_index];
        UnitLabIedModelReportControl* model_report = &plan->reports[report_index];
        size_t data_set_index = 0U;
        if (!find_data_set_index(fixture, report->data_set_ref, &data_set_index)) {
            set_error(error, error_size, "MODEL_PLAN_REPORT_DATASET_NOT_FOUND: %s", report->data_set_ref);
            unitlab_free_ied_model_plan(plan);
            return 0;
        }
        if (!add_node_reference(plan, report->logical_device_inst, report->logical_node_name)) {
            set_error(error, error_size, "MODEL_PLAN_REPORT_NODE_TOO_LONG: %s/%s", report->logical_device_inst, report->logical_node_name);
            unitlab_free_ied_model_plan(plan);
            return 0;
        }
        if (!copy_string(model_report->key, sizeof(model_report->key), report->key)) {
            set_error(error, error_size, "MODEL_PLAN_REPORT_KEY_TOO_LONG: %s", report->key);
            unitlab_free_ied_model_plan(plan);
            return 0;
        }
        if (!copy_string(model_report->data_set_ref, sizeof(model_report->data_set_ref), report->data_set_ref)) {
            set_error(error, error_size, "MODEL_PLAN_REPORT_DATASET_TOO_LONG: %s", report->data_set_ref);
            unitlab_free_ied_model_plan(plan);
            return 0;
        }
        model_report->data_set_index = data_set_index;
        plan->report_count++;
    }

    return 1;
}

void unitlab_free_ied_model_plan(UnitLabIedModelPlan* plan)
{
    if (plan == NULL) {
        return;
    }
    free(plan->logical_devices);
    free(plan->logical_nodes);
    free(plan->data_sets);
    free(plan->reports);
    memset(plan, 0, sizeof(*plan));
}
