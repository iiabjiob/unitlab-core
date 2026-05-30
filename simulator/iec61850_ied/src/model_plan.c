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

static int normalize_object_reference(
    const char* source,
    size_t source_length,
    char* destination,
    size_t destination_size)
{
    if (source_length == 0U || source_length >= destination_size) {
        return 0;
    }
    for (size_t index = 0U; index < source_length; index++) {
        char character = source[index];
        if (character == '/') {
            character = '.';
        }
        destination[index] = character;
    }
    destination[source_length] = '\0';
    return 1;
}

static int parse_fc_suffix(
    const UnitLabIedFixtureSignal* signal,
    const char* body_end,
    char* fc,
    size_t fc_size,
    const char** reference_body_end,
    char* error,
    size_t error_size)
{
    const char* bracket = strrchr(signal->reference, '[');
    const char* parsed_fc = signal->fc;
    size_t parsed_fc_length = strlen(parsed_fc);

    *reference_body_end = body_end;
    if (bracket != NULL) {
        const char* close = strchr(bracket, ']');
        if (close == NULL || close[1] != '\0' || close == bracket + 1) {
            set_error(error, error_size, "MODEL_PLAN_SIGNAL_FC_INVALID: %s", signal->reference);
            return 0;
        }
        if (parsed_fc_length > 0U) {
            size_t bracket_fc_length = (size_t)(close - bracket - 1);
            if (parsed_fc_length != bracket_fc_length || strncmp(parsed_fc, bracket + 1, bracket_fc_length) != 0) {
                set_error(error, error_size, "MODEL_PLAN_SIGNAL_FC_MISMATCH: %s", signal->reference);
                return 0;
            }
        }
        parsed_fc = bracket + 1;
        parsed_fc_length = (size_t)(close - bracket - 1);
        *reference_body_end = bracket;
    }

    if (parsed_fc_length == 0U || parsed_fc_length >= fc_size) {
        set_error(error, error_size, "MODEL_PLAN_SIGNAL_FC_MISSING: %s", signal->reference);
        return 0;
    }
    memcpy(fc, parsed_fc, parsed_fc_length);
    fc[parsed_fc_length] = '\0';
    return 1;
}

static int parse_signal_reference(
    const UnitLabIedFixtureSignal* signal,
    UnitLabIedModelSignal* model_signal,
    char* error,
    size_t error_size)
{
    const char* reference = signal->reference;
    const char* reference_end = reference + strlen(reference);
    const char* reference_body_end = reference_end;
    const char* separator = strchr(reference, '/');
    if (separator == NULL || separator == reference) {
        set_error(error, error_size, "MODEL_PLAN_SIGNAL_REFERENCE_INVALID: %s", reference);
        return 0;
    }

    if (!parse_fc_suffix(signal, reference_end, model_signal->fc, sizeof(model_signal->fc), &reference_body_end, error, error_size)) {
        return 0;
    }

    const char* node_start = separator + 1;
    const char* dot = strchr(node_start, '.');
    const char* slash = strchr(node_start, '/');
    const char* node_end = dot;
    if (node_end == NULL || (slash != NULL && slash < node_end)) {
        node_end = slash;
    }
    if (node_end == NULL || node_end == node_start || node_end >= reference_body_end) {
        set_error(error, error_size, "MODEL_PLAN_SIGNAL_REFERENCE_INVALID: %s", reference);
        return 0;
    }

    size_t logical_device_length = (size_t)(separator - reference);
    size_t logical_node_length = (size_t)(node_end - node_start);
    const char* object_start = node_end + 1;
    size_t object_length = (size_t)(reference_body_end - object_start);
    if (
        logical_device_length >= sizeof(model_signal->logical_device_inst)
        || logical_node_length >= sizeof(model_signal->logical_node_name)
        || object_length == 0U
    ) {
        set_error(error, error_size, "MODEL_PLAN_SIGNAL_REFERENCE_INVALID: %s", reference);
        return 0;
    }

    if (!copy_string(model_signal->reference, sizeof(model_signal->reference), reference)) {
        set_error(error, error_size, "MODEL_PLAN_SIGNAL_REFERENCE_TOO_LONG: %s", reference);
        return 0;
    }
    memcpy(model_signal->logical_device_inst, reference, logical_device_length);
    model_signal->logical_device_inst[logical_device_length] = '\0';
    memcpy(model_signal->logical_node_name, node_start, logical_node_length);
    model_signal->logical_node_name[logical_node_length] = '\0';
    if (!normalize_object_reference(object_start, object_length, model_signal->object_reference, sizeof(model_signal->object_reference))) {
        set_error(error, error_size, "MODEL_PLAN_SIGNAL_OBJECT_TOO_LONG: %s", reference);
        return 0;
    }
    if (!copy_string(model_signal->initial_value, sizeof(model_signal->initial_value), signal->initial_value)) {
        set_error(error, error_size, "MODEL_PLAN_SIGNAL_VALUE_TOO_LONG: %s", reference);
        return 0;
    }
    return 1;
}

static int parse_data_set_reference(
    const UnitLabIedFixtureModel* fixture,
    const char* reference,
    UnitLabIedModelDataSet* data_set,
    char* error,
    size_t error_size)
{
    const char* first_slash = strchr(reference, '/');
    if (first_slash == NULL || first_slash == reference) {
        set_error(error, error_size, "MODEL_PLAN_DATASET_REFERENCE_INVALID: %s", reference);
        return 0;
    }
    const char* second_slash = strchr(first_slash + 1, '/');
    if (second_slash == NULL || second_slash == first_slash + 1) {
        set_error(error, error_size, "MODEL_PLAN_DATASET_REFERENCE_INVALID: %s", reference);
        return 0;
    }
    const char* third_slash = strchr(second_slash + 1, '/');
    if (third_slash == NULL || third_slash == second_slash + 1) {
        set_error(error, error_size, "MODEL_PLAN_DATASET_REFERENCE_INVALID: %s", reference);
        return 0;
    }
    const char* dot = strchr(third_slash + 1, '.');
    if (dot == NULL || dot == third_slash + 1 || dot[1] == '\0') {
        set_error(error, error_size, "MODEL_PLAN_DATASET_REFERENCE_INVALID: %s", reference);
        return 0;
    }

    size_t ied_length = (size_t)(first_slash - reference);
    size_t access_point_length = (size_t)(second_slash - first_slash - 1);
    if (
        strlen(fixture->ied_name) != ied_length
        || strncmp(fixture->ied_name, reference, ied_length) != 0
        || strlen(fixture->access_point_name) != access_point_length
        || strncmp(fixture->access_point_name, first_slash + 1, access_point_length) != 0
    ) {
        set_error(error, error_size, "MODEL_PLAN_DATASET_CONTEXT_MISMATCH: %s", reference);
        return 0;
    }

    size_t logical_device_length = (size_t)(third_slash - second_slash - 1);
    size_t logical_node_length = (size_t)(dot - third_slash - 1);
    const char* data_set_name = dot + 1;
    if (
        logical_device_length >= sizeof(data_set->logical_device_inst)
        || logical_node_length >= sizeof(data_set->logical_node_name)
        || strlen(data_set_name) >= sizeof(data_set->name)
    ) {
        set_error(error, error_size, "MODEL_PLAN_DATASET_REFERENCE_TOO_LONG: %s", reference);
        return 0;
    }

    if (!copy_string(data_set->reference, sizeof(data_set->reference), reference)) {
        set_error(error, error_size, "MODEL_PLAN_DATASET_REFERENCE_TOO_LONG: %s", reference);
        return 0;
    }
    memcpy(data_set->logical_device_inst, second_slash + 1, logical_device_length);
    data_set->logical_device_inst[logical_device_length] = '\0';
    memcpy(data_set->logical_node_name, third_slash + 1, logical_node_length);
    data_set->logical_node_name[logical_node_length] = '\0';
    if (!copy_string(data_set->name, sizeof(data_set->name), data_set_name)) {
        set_error(error, error_size, "MODEL_PLAN_DATASET_REFERENCE_TOO_LONG: %s", reference);
        return 0;
    }
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
    plan->signals = (UnitLabIedModelSignal*)calloc(fixture->signal_count, sizeof(UnitLabIedModelSignal));
    if (
        plan->logical_devices == NULL
        || plan->logical_nodes == NULL
        || plan->data_sets == NULL
        || plan->reports == NULL
        || plan->signals == NULL
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
        if (!parse_data_set_reference(fixture, data_set->reference, model_data_set, error, error_size)) {
            unitlab_free_ied_model_plan(plan);
            return 0;
        }
        if (!add_node_reference(plan, model_data_set->logical_device_inst, model_data_set->logical_node_name)) {
            set_error(error, error_size, "MODEL_PLAN_DATASET_NODE_TOO_LONG: %s/%s", model_data_set->logical_device_inst, model_data_set->logical_node_name);
            unitlab_free_ied_model_plan(plan);
            return 0;
        }
        model_data_set->first_signal_index = plan->signal_count;
        model_data_set->member_count = data_set->signal_count;
        plan->data_set_count++;

        for (size_t signal_index = 0U; signal_index < data_set->signal_count; signal_index++) {
            const UnitLabIedFixtureSignal* signal = &data_set->signals[signal_index];
            UnitLabIedModelSignal* model_signal = &plan->signals[plan->signal_count];
            if (signal->data_set_index != signal_index) {
                set_error(error, error_size, "MODEL_PLAN_SIGNAL_INDEX_ORDER_INVALID: %s", signal->reference);
                unitlab_free_ied_model_plan(plan);
                return 0;
            }
            if (!parse_signal_reference(signal, model_signal, error, error_size)) {
                unitlab_free_ied_model_plan(plan);
                return 0;
            }
            model_signal->data_set_index = data_set_index;
            model_signal->member_index = signal_index;
            if (!add_node_reference(plan, model_signal->logical_device_inst, model_signal->logical_node_name)) {
                set_error(error, error_size, "MODEL_PLAN_NODE_REFERENCE_TOO_LONG: %s/%s", model_signal->logical_device_inst, model_signal->logical_node_name);
                unitlab_free_ied_model_plan(plan);
                return 0;
            }
            plan->signal_count++;
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
        if (!copy_string(model_report->logical_device_inst, sizeof(model_report->logical_device_inst), report->logical_device_inst)) {
            set_error(error, error_size, "MODEL_PLAN_REPORT_LD_TOO_LONG: %s", report->logical_device_inst);
            unitlab_free_ied_model_plan(plan);
            return 0;
        }
        if (!copy_string(model_report->logical_node_name, sizeof(model_report->logical_node_name), report->logical_node_name)) {
            set_error(error, error_size, "MODEL_PLAN_REPORT_LN_TOO_LONG: %s", report->logical_node_name);
            unitlab_free_ied_model_plan(plan);
            return 0;
        }
        if (!copy_string(model_report->name, sizeof(model_report->name), report->report_control_name)) {
            set_error(error, error_size, "MODEL_PLAN_REPORT_NAME_TOO_LONG: %s", report->report_control_name);
            unitlab_free_ied_model_plan(plan);
            return 0;
        }
        if (!copy_string(model_report->report_kind, sizeof(model_report->report_kind), report->report_kind)) {
            set_error(error, error_size, "MODEL_PLAN_REPORT_KIND_TOO_LONG: %s", report->report_kind);
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
    free(plan->signals);
    memset(plan, 0, sizeof(*plan));
}
