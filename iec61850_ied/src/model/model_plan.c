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

static int format_data_set_entry_variable(
    const char* logical_device_inst,
    const char* logical_node_name,
    const char* fc,
    const char* object_reference,
    char* destination,
    size_t destination_size)
{
    if (
        logical_device_inst == NULL
        || logical_node_name == NULL
        || fc == NULL
        || object_reference == NULL
        || destination == NULL
        || destination_size == 0U
    ) {
        return 0;
    }

    int written = snprintf(destination, destination_size, "%s/%s$%s$", logical_device_inst, logical_node_name, fc);
    if (written <= 0 || (size_t)written >= destination_size) {
        return 0;
    }

    size_t used = (size_t)written;
    for (const char* cursor = object_reference; *cursor != '\0'; cursor++) {
        if (used + 1U >= destination_size) {
            return 0;
        }
        destination[used++] = *cursor == '.' ? '$' : *cursor;
    }
    destination[used] = '\0';
    return 1;
}

static int parse_object_path(
    const char* object_reference,
    char* data_object_name,
    size_t data_object_name_size,
    char* data_attribute_path,
    size_t data_attribute_path_size)
{
    const char* dot = strchr(object_reference, '.');
    size_t data_object_length = dot == NULL ? strlen(object_reference) : (size_t)(dot - object_reference);
    if (data_object_length == 0U || data_object_length >= data_object_name_size) {
        return 0;
    }

    memcpy(data_object_name, object_reference, data_object_length);
    data_object_name[data_object_length] = '\0';

    if (dot == NULL) {
        if (data_attribute_path_size == 0U) {
            return 0;
        }
        data_attribute_path[0] = '\0';
        return 1;
    }

    const char* data_attribute = dot + 1;
    size_t data_attribute_length = strlen(data_attribute);
    if (data_attribute_length == 0U || data_attribute_length >= data_attribute_path_size) {
        return 0;
    }
    memcpy(data_attribute_path, data_attribute, data_attribute_length + 1U);
    return 1;
}

static int validate_signal_kind(const UnitLabIedFixtureSignal* signal, char* error, size_t error_size)
{
    if (strcmp(signal->kind, "FCD") == 0 || strcmp(signal->kind, "FCDA") == 0) {
        return 1;
    }
    set_error(error, error_size, "MODEL_PLAN_SIGNAL_KIND_INVALID: %s", signal->reference);
    return 0;
}

static int parse_uint32_string(const char* source, int* known, uint32_t* value)
{
    if (known == NULL || value == NULL) {
        return 0;
    }
    *known = 0;
    *value = 0U;
    if (source == NULL || source[0] == '\0') {
        return 1;
    }

    unsigned long parsed = 0U;
    for (const char* cursor = source; *cursor != '\0'; cursor++) {
        if (*cursor < '0' || *cursor > '9') {
            return 0;
        }
        parsed = (parsed * 10UL) + (unsigned long)(*cursor - '0');
        if (parsed > 4294967295UL) {
            return 0;
        }
    }
    *known = 1;
    *value = (uint32_t)parsed;
    return 1;
}

static int copy_optional_uint32(int source_known, int source_value, int* target_known, uint32_t* target_value)
{
    if (target_known == NULL || target_value == NULL || source_value < 0) {
        return 0;
    }
    *target_known = source_known;
    *target_value = source_known ? (uint32_t)source_value : 0U;
    return 1;
}

static int derive_report_buffered_flag(
    const UnitLabIedFixtureReport* report,
    UnitLabIedModelReportControl* model_report,
    char* error,
    size_t error_size)
{
    if (strcmp(report->report_kind, "buffered") == 0) {
        model_report->is_buffered = 1;
        return 1;
    }
    if (strcmp(report->report_kind, "unbuffered") == 0) {
        model_report->is_buffered = 0;
        return 1;
    }
    set_error(error, error_size, "MODEL_PLAN_REPORT_KIND_INVALID: %s", report->report_kind);
    return 0;
}

static void append_optional_bool_mask(UnitLabIedFixtureOptionalBool field, uint8_t bit, uint8_t* mask)
{
    if (field.known && field.value) {
        *mask = (uint8_t)(*mask | bit);
    }
}

static uint8_t build_trigger_options_mask(const UnitLabIedFixtureTriggerOptions* options)
{
    uint8_t mask = 0U;
    append_optional_bool_mask(options->data_change, UNITLAB_IED_MODEL_TRG_OPT_DATA_CHANGED, &mask);
    append_optional_bool_mask(options->quality_change, UNITLAB_IED_MODEL_TRG_OPT_QUALITY_CHANGED, &mask);
    append_optional_bool_mask(options->data_update, UNITLAB_IED_MODEL_TRG_OPT_DATA_UPDATE, &mask);
    append_optional_bool_mask(options->periodic, UNITLAB_IED_MODEL_TRG_OPT_INTEGRITY, &mask);
    append_optional_bool_mask(options->general_interrogation, UNITLAB_IED_MODEL_TRG_OPT_GI, &mask);
    return mask;
}

static uint8_t build_optional_fields_mask(const UnitLabIedFixtureOptionalFields* fields)
{
    uint8_t mask = 0U;
    append_optional_bool_mask(fields->sequence_number, UNITLAB_IED_MODEL_RPT_OPT_SEQ_NUM, &mask);
    append_optional_bool_mask(fields->timestamp, UNITLAB_IED_MODEL_RPT_OPT_TIME_STAMP, &mask);
    append_optional_bool_mask(fields->reason_code, UNITLAB_IED_MODEL_RPT_OPT_REASON_FOR_INCLUSION, &mask);
    append_optional_bool_mask(fields->data_set_name, UNITLAB_IED_MODEL_RPT_OPT_DATA_SET, &mask);
    append_optional_bool_mask(fields->data_reference, UNITLAB_IED_MODEL_RPT_OPT_DATA_REFERENCE, &mask);
    append_optional_bool_mask(fields->buffer_overflow, UNITLAB_IED_MODEL_RPT_OPT_BUFFER_OVERFLOW, &mask);
    append_optional_bool_mask(fields->entry_id, UNITLAB_IED_MODEL_RPT_OPT_ENTRY_ID, &mask);
    append_optional_bool_mask(fields->config_revision, UNITLAB_IED_MODEL_RPT_OPT_CONF_REV, &mask);
    return mask;
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
    if (!validate_signal_kind(signal, error, error_size)) {
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
    if (!copy_string(model_signal->kind, sizeof(model_signal->kind), signal->kind)) {
        set_error(error, error_size, "MODEL_PLAN_SIGNAL_KIND_TOO_LONG: %s", reference);
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
    if (!parse_object_path(
            model_signal->object_reference,
            model_signal->data_object_name,
            sizeof(model_signal->data_object_name),
            model_signal->data_attribute_path,
            sizeof(model_signal->data_attribute_path))) {
        set_error(error, error_size, "MODEL_PLAN_SIGNAL_OBJECT_INVALID: %s", reference);
        return 0;
    }
    if (!format_data_set_entry_variable(
            model_signal->logical_device_inst,
            model_signal->logical_node_name,
            model_signal->fc,
            model_signal->object_reference,
            model_signal->data_set_entry_variable,
            sizeof(model_signal->data_set_entry_variable))) {
        set_error(error, error_size, "MODEL_PLAN_SIGNAL_DATASET_ENTRY_TOO_LONG: %s", reference);
        return 0;
    }
    model_signal->data_set_entry_component_known = signal->component[0] != '\0';
    if (model_signal->data_set_entry_component_known) {
        if (!copy_string(
                model_signal->data_set_entry_component,
                sizeof(model_signal->data_set_entry_component),
                signal->component)) {
            set_error(error, error_size, "MODEL_PLAN_SIGNAL_DATASET_ENTRY_COMPONENT_TOO_LONG: %s", reference);
            return 0;
        }
    }
    else {
        model_signal->data_set_entry_component[0] = '\0';
    }
    if (!copy_string(model_signal->initial_value, sizeof(model_signal->initial_value), signal->initial_value)) {
        set_error(error, error_size, "MODEL_PLAN_SIGNAL_VALUE_TOO_LONG: %s", reference);
        return 0;
    }
    if (signal->initial_value_kind == UNITLAB_IED_FIXTURE_VALUE_UNKNOWN) {
        set_error(error, error_size, "MODEL_PLAN_SIGNAL_VALUE_KIND_UNKNOWN: %s", reference);
        return 0;
    }
    model_signal->initial_value_kind = signal->initial_value_kind;
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

static int has_namespace_attribute(
    const UnitLabIedModelPlan* plan,
    const char* logical_device_inst,
    const char* logical_node_name,
    const char* name)
{
    for (size_t index = 0U; index < plan->namespace_attribute_count; index++) {
        const UnitLabIedModelNamespaceAttribute* attribute = &plan->namespace_attributes[index];
        if (
            strcmp(attribute->logical_device_inst, logical_device_inst) == 0
            && strcmp(attribute->logical_node_name, logical_node_name) == 0
            && strcmp(attribute->name, name) == 0
        ) {
            return 1;
        }
    }
    return 0;
}

static int add_namespace_attribute(
    UnitLabIedModelPlan* plan,
    const char* logical_device_inst,
    const char* logical_node_name,
    const char* data_object_name,
    const char* name,
    UnitLabIedFixtureValueKind initial_value_kind,
    const char* initial_value)
{
    UnitLabIedModelNamespaceAttribute* attribute = NULL;

    if (has_namespace_attribute(plan, logical_device_inst, logical_node_name, name)) {
        return 1;
    }
    attribute = &plan->namespace_attributes[plan->namespace_attribute_count];
    if (!copy_string(attribute->logical_device_inst, sizeof(attribute->logical_device_inst), logical_device_inst)) {
        return 0;
    }
    if (!copy_string(attribute->logical_node_name, sizeof(attribute->logical_node_name), logical_node_name)) {
        return 0;
    }
    if (!copy_string(attribute->data_object_name, sizeof(attribute->data_object_name), data_object_name)) {
        return 0;
    }
    if (!copy_string(attribute->name, sizeof(attribute->name), name)) {
        return 0;
    }
    if (snprintf(attribute->object_reference, sizeof(attribute->object_reference), "%s.%s.EX.%s.%s", logical_device_inst, logical_node_name, data_object_name, name) >= (int)sizeof(attribute->object_reference)) {
        return 0;
    }
    attribute->initial_value_kind = initial_value_kind;
    if (!copy_string(attribute->initial_value, sizeof(attribute->initial_value), initial_value)) {
        return 0;
    }
    plan->namespace_attribute_count++;
    return 1;
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
    size_t max_namespace_attributes = max_logical_nodes * 4U;
    if (max_logical_devices == 0U || max_logical_nodes == 0U) {
        set_error(error, error_size, "MODEL_PLAN_EMPTY: fixture has no signals or reports.");
        return 0;
    }

    plan->logical_devices = (UnitLabIedModelLogicalDevice*)calloc(max_logical_devices, sizeof(UnitLabIedModelLogicalDevice));
    plan->logical_nodes = (UnitLabIedModelLogicalNode*)calloc(max_logical_nodes, sizeof(UnitLabIedModelLogicalNode));
    plan->data_sets = (UnitLabIedModelDataSet*)calloc(fixture->data_set_count, sizeof(UnitLabIedModelDataSet));
    plan->reports = (UnitLabIedModelReportControl*)calloc(fixture->report_count, sizeof(UnitLabIedModelReportControl));
    plan->signals = (UnitLabIedModelSignal*)calloc(fixture->signal_count, sizeof(UnitLabIedModelSignal));
    plan->namespace_attributes = (UnitLabIedModelNamespaceAttribute*)calloc(max_namespace_attributes, sizeof(UnitLabIedModelNamespaceAttribute));
    if (
        plan->logical_devices == NULL
        || plan->logical_nodes == NULL
        || plan->data_sets == NULL
        || plan->reports == NULL
        || plan->signals == NULL
        || plan->namespace_attributes == NULL
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
        if (!derive_report_buffered_flag(report, model_report, error, error_size)) {
            unitlab_free_ied_model_plan(plan);
            return 0;
        }
        if (!copy_string(model_report->rpt_id, sizeof(model_report->rpt_id), report->rpt_id)) {
            set_error(error, error_size, "MODEL_PLAN_REPORT_RPTID_TOO_LONG: %s", report->rpt_id);
            unitlab_free_ied_model_plan(plan);
            return 0;
        }
        if (!copy_string(model_report->data_set_ref, sizeof(model_report->data_set_ref), report->data_set_ref)) {
            set_error(error, error_size, "MODEL_PLAN_REPORT_DATASET_TOO_LONG: %s", report->data_set_ref);
            unitlab_free_ied_model_plan(plan);
            return 0;
        }
        if (!parse_uint32_string(report->conf_rev, &model_report->conf_rev_known, &model_report->conf_rev)) {
            set_error(error, error_size, "MODEL_PLAN_REPORT_CONFREV_INVALID: %s", report->key);
            unitlab_free_ied_model_plan(plan);
            return 0;
        }
        if (!copy_optional_uint32(report->buffer_time_ms_known, report->buffer_time_ms, &model_report->buffer_time_ms_known, &model_report->buffer_time_ms)) {
            set_error(error, error_size, "MODEL_PLAN_REPORT_BUFTM_INVALID: %s", report->key);
            unitlab_free_ied_model_plan(plan);
            return 0;
        }
        if (!copy_optional_uint32(
                report->integrity_period_ms_known,
                report->integrity_period_ms,
                &model_report->integrity_period_ms_known,
                &model_report->integrity_period_ms)) {
            set_error(error, error_size, "MODEL_PLAN_REPORT_INTGPD_INVALID: %s", report->key);
            unitlab_free_ied_model_plan(plan);
            return 0;
        }
        model_report->data_set_index = data_set_index;
        model_report->indexed_known = report->indexed_known;
        model_report->indexed = report->indexed;
        model_report->trigger_options = report->trigger_options;
        model_report->optional_fields = report->optional_fields;
        model_report->trigger_options_mask = build_trigger_options_mask(&report->trigger_options);
        model_report->optional_fields_mask = build_optional_fields_mask(&report->optional_fields);
        plan->report_count++;
    }

    for (size_t node_index = 0U; node_index < plan->logical_node_count; node_index++) {
        const UnitLabIedModelLogicalNode* node = &plan->logical_nodes[node_index];
        if (strcmp(node->name, "LLN0") != 0) {
            continue;
        }
        if (!add_namespace_attribute(plan, node->logical_device_inst, node->name, "NamPlt", "ldNs", UNITLAB_IED_FIXTURE_VALUE_STRING, "LD0")) {
            set_error(error, error_size, "MODEL_PLAN_NAMESPACE_ATTRIBUTE_TOO_LONG: %s/%s/ldNs", node->logical_device_inst, node->name);
            unitlab_free_ied_model_plan(plan);
            return 0;
        }
        if (!add_namespace_attribute(plan, node->logical_device_inst, node->name, "NamPlt", "lnNs", UNITLAB_IED_FIXTURE_VALUE_STRING, "IEC 61850-7-4:2007")) {
            set_error(error, error_size, "MODEL_PLAN_NAMESPACE_ATTRIBUTE_TOO_LONG: %s/%s/lnNs", node->logical_device_inst, node->name);
            unitlab_free_ied_model_plan(plan);
            return 0;
        }
        if (!add_namespace_attribute(plan, node->logical_device_inst, node->name, "NamPlt", "cdcNs", UNITLAB_IED_FIXTURE_VALUE_STRING, "IEC 61850-7-3:2010")) {
            set_error(error, error_size, "MODEL_PLAN_NAMESPACE_ATTRIBUTE_TOO_LONG: %s/%s/cdcNs", node->logical_device_inst, node->name);
            unitlab_free_ied_model_plan(plan);
            return 0;
        }
        if (!add_namespace_attribute(plan, node->logical_device_inst, node->name, "NamPlt", "dataNs", UNITLAB_IED_FIXTURE_VALUE_STRING, "EXT:2015")) {
            set_error(error, error_size, "MODEL_PLAN_NAMESPACE_ATTRIBUTE_TOO_LONG: %s/%s/dataNs", node->logical_device_inst, node->name);
            unitlab_free_ied_model_plan(plan);
            return 0;
        }
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
    free(plan->namespace_attributes);
    memset(plan, 0, sizeof(*plan));
}

static int append_metadata_name(char*** names, size_t* count, const char* name)
{
    char** next;
    char* copy;
    size_t length;

    if (names == NULL || count == NULL || name == NULL) {
        return 0;
    }
    length = strlen(name);
    next = (char**)realloc(*names, (*count + 1U) * sizeof(char*));
    if (next == NULL) {
        return 0;
    }
    copy = (char*)malloc(length + 1U);
    if (copy == NULL) {
        free(next);
        return 0;
    }
    if (!copy_string(copy, length + 1U, name)) {
        free(copy);
        free(next);
        return 0;
    }
    next[*count] = copy;
    *names = next;
    *count += 1U;
    return 1;
}

static int append_unique_metadata_name(char*** names, size_t* count, const char* name)
{
    if (names == NULL || count == NULL || name == NULL) {
        return 0;
    }
    for (size_t index = 0U; index < *count; index++) {
        if (strcmp((*names)[index], name) == 0) {
            return 1;
        }
    }
    return append_metadata_name(names, count, name);
}

int unitlab_collect_ied_model_logical_devices(
    const UnitLabIedModelPlan* plan,
    char*** names,
    size_t* count,
    char* error,
    size_t error_size)
{
    if (names != NULL) {
        *names = NULL;
    }
    if (count != NULL) {
        *count = 0U;
    }
    if (plan == NULL || names == NULL || count == NULL) {
        set_error(error, error_size, "INVALID_ARGUMENT: plan, names, and count are required.");
        return 0;
    }
    for (size_t index = 0U; index < plan->logical_device_count; index++) {
        if (!append_metadata_name(names, count, plan->logical_devices[index].inst)) {
            unitlab_free_ied_model_name_list(*names, *count);
            *names = NULL;
            *count = 0U;
            set_error(error, error_size, "OUT_OF_MEMORY: cannot collect logical devices.");
            return 0;
        }
    }
    return 1;
}

int unitlab_collect_ied_model_logical_node_data_sets(
    const UnitLabIedModelPlan* plan,
    const char* logical_device_inst,
    const char* logical_node_name,
    char*** names,
    size_t* count,
    char* error,
    size_t error_size)
{
    if (names != NULL) {
        *names = NULL;
    }
    if (count != NULL) {
        *count = 0U;
    }
    if (plan == NULL || logical_device_inst == NULL || logical_node_name == NULL || names == NULL || count == NULL) {
        set_error(error, error_size, "INVALID_ARGUMENT: plan, logical device, logical node, names, and count are required.");
        return 0;
    }
    for (size_t index = 0U; index < plan->data_set_count; index++) {
        const UnitLabIedModelDataSet* data_set = &plan->data_sets[index];
        if (strcmp(data_set->logical_device_inst, logical_device_inst) != 0 || strcmp(data_set->logical_node_name, logical_node_name) != 0) {
            continue;
        }
        if (!append_metadata_name(names, count, data_set->name)) {
            unitlab_free_ied_model_name_list(*names, *count);
            *names = NULL;
            *count = 0U;
            set_error(error, error_size, "OUT_OF_MEMORY: cannot collect DataSets.");
            return 0;
        }
    }
    return 1;
}

int unitlab_collect_ied_model_logical_device_data_sets(
    const UnitLabIedModelPlan* plan,
    const char* logical_device_inst,
    char*** names,
    size_t* count,
    char* error,
    size_t error_size)
{
    if (names != NULL) {
        *names = NULL;
    }
    if (count != NULL) {
        *count = 0U;
    }
    if (plan == NULL || logical_device_inst == NULL || logical_device_inst[0] == '\0' || names == NULL || count == NULL) {
        set_error(error, error_size, "INVALID_ARGUMENT: plan, logical device, names, and count are required.");
        return 0;
    }
    for (size_t node_index = 0U; node_index < plan->logical_node_count; node_index++) {
        const UnitLabIedModelLogicalNode* logical_node = &plan->logical_nodes[node_index];

        if (strcmp(logical_node->logical_device_inst, logical_device_inst) != 0) {
            continue;
        }

        for (size_t data_set_index = 0U; data_set_index < plan->data_set_count; data_set_index++) {
            const UnitLabIedModelDataSet* data_set = &plan->data_sets[data_set_index];
            size_t qualified_length;
            char* qualified_name;

            if (strcmp(data_set->logical_device_inst, logical_device_inst) != 0 || strcmp(data_set->logical_node_name, logical_node->name) != 0) {
                continue;
            }
            qualified_length = strlen(logical_node->name) + 1U + strlen(data_set->name) + 1U;
            qualified_name = (char*)calloc(qualified_length, sizeof(char));
            if (qualified_name == NULL) {
                unitlab_free_ied_model_name_list(*names, *count);
                *names = NULL;
                *count = 0U;
                set_error(error, error_size, "OUT_OF_MEMORY: cannot collect logical-device DataSets.");
                return 0;
            }
            snprintf(qualified_name, qualified_length, "%s$%s", logical_node->name, data_set->name);
            if (!append_metadata_name(names, count, qualified_name)) {
                free(qualified_name);
                unitlab_free_ied_model_name_list(*names, *count);
                *names = NULL;
                *count = 0U;
                set_error(error, error_size, "OUT_OF_MEMORY: cannot collect logical-device DataSets.");
                return 0;
            }
        }
    }
    return 1;
}

int unitlab_collect_ied_model_vmd_named_variable_lists(
    const UnitLabIedModelPlan* plan,
    char*** names,
    size_t* count,
    char* error,
    size_t error_size)
{
    if (names != NULL) {
        *names = NULL;
    }
    if (count != NULL) {
        *count = 0U;
    }
    if (plan == NULL || names == NULL || count == NULL) {
        set_error(error, error_size, "INVALID_ARGUMENT: plan, names, and count are required.");
        return 0;
    }

    for (size_t index = 0U; index < plan->data_set_count; index++) {
        const UnitLabIedModelDataSet* data_set = &plan->data_sets[index];

        if (data_set->name[0] == '\0') {
            continue;
        }
        if (!append_unique_metadata_name(names, count, data_set->name)) {
            unitlab_free_ied_model_name_list(*names, *count);
            *names = NULL;
            *count = 0U;
            set_error(error, error_size, "OUT_OF_MEMORY: cannot collect VMD-specific NamedVariableLists.");
            return 0;
        }
    }
    return 1;
}

int unitlab_collect_ied_model_logical_device_variables(
    const UnitLabIedModelPlan* plan,
    const char* logical_device_inst,
    char*** names,
    size_t* count,
    char* error,
    size_t error_size)
{
    if (names != NULL) {
        *names = NULL;
    }
    if (count != NULL) {
        *count = 0U;
    }
    if (plan == NULL || logical_device_inst == NULL || logical_device_inst[0] == '\0' || names == NULL || count == NULL) {
        set_error(error, error_size, "INVALID_ARGUMENT: plan, logical device, names, and count are required.");
        return 0;
    }

    for (size_t node_index = 0U; node_index < plan->logical_node_count; node_index++) {
        const UnitLabIedModelLogicalNode* logical_node = &plan->logical_nodes[node_index];

        if (strcmp(logical_node->logical_device_inst, logical_device_inst) != 0) {
            continue;
        }
        if (!append_unique_metadata_name(names, count, logical_node->name)) {
            unitlab_free_ied_model_name_list(*names, *count);
            *names = NULL;
            *count = 0U;
            set_error(error, error_size, "OUT_OF_MEMORY: cannot collect NamedVariables.");
            return 0;
        }
    }

    return 1;
}

int unitlab_collect_ied_model_logical_node_variables(
    const UnitLabIedModelPlan* plan,
    const char* logical_device_inst,
    const char* logical_node_name,
    char*** names,
    size_t* count,
    char* error,
    size_t error_size)
{
    static const char* const lln0_common_variables[] = { "Mod", "Beh", "Health", "CF", "DC", "BR", "EX" };

    if (names != NULL) {
        *names = NULL;
    }
    if (count != NULL) {
        *count = 0U;
    }
    if (plan == NULL || logical_device_inst == NULL || logical_node_name == NULL || logical_node_name[0] == '\0' || names == NULL || count == NULL) {
        set_error(error, error_size, "INVALID_ARGUMENT: plan, logical device, logical node, names, and count are required.");
        return 0;
    }

    if (strcmp(logical_node_name, "LLN0") == 0) {
        for (size_t index = 0U; index < sizeof(lln0_common_variables) / sizeof(lln0_common_variables[0]); index++) {
            if (!append_unique_metadata_name(names, count, lln0_common_variables[index])) {
                unitlab_free_ied_model_name_list(*names, *count);
                *names = NULL;
                *count = 0U;
                set_error(error, error_size, "OUT_OF_MEMORY: cannot collect logical-node variables.");
                return 0;
            }
        }
    }

    for (size_t signal_index = 0U; signal_index < plan->signal_count; signal_index++) {
        const UnitLabIedModelSignal* signal = &plan->signals[signal_index];

        if (strcmp(signal->logical_device_inst, logical_device_inst) != 0 || strcmp(signal->logical_node_name, logical_node_name) != 0) {
            continue;
        }
        if (signal->data_object_name[0] == '\0') {
            continue;
        }
        if (!append_unique_metadata_name(names, count, signal->data_object_name)) {
            unitlab_free_ied_model_name_list(*names, *count);
            *names = NULL;
            *count = 0U;
            set_error(error, error_size, "OUT_OF_MEMORY: cannot collect logical-node variables.");
            return 0;
        }
    }
    return 1;
}

int unitlab_collect_ied_model_logical_node_namespace_attributes(
    const UnitLabIedModelPlan* plan,
    const char* logical_device_inst,
    const char* logical_node_name,
    char*** names,
    size_t* count,
    char* error,
    size_t error_size)
{
    if (names != NULL) {
        *names = NULL;
    }
    if (count != NULL) {
        *count = 0U;
    }
    if (plan == NULL || logical_device_inst == NULL || logical_node_name == NULL || names == NULL || count == NULL) {
        set_error(error, error_size, "INVALID_ARGUMENT: plan, logical device, logical node, names, and count are required.");
        return 0;
    }
    for (size_t index = 0U; index < plan->namespace_attribute_count; index++) {
        const UnitLabIedModelNamespaceAttribute* attribute = &plan->namespace_attributes[index];
        if (strcmp(attribute->logical_device_inst, logical_device_inst) != 0 || strcmp(attribute->logical_node_name, logical_node_name) != 0) {
            continue;
        }
        if (!append_unique_metadata_name(names, count, attribute->name)) {
            unitlab_free_ied_model_name_list(*names, *count);
            *names = NULL;
            *count = 0U;
            set_error(error, error_size, "OUT_OF_MEMORY: cannot collect logical-node namespace attributes.");
            return 0;
        }
    }
    return 1;
}

int unitlab_collect_ied_model_logical_node_reports(
    const UnitLabIedModelPlan* plan,
    const char* logical_device_inst,
    const char* logical_node_name,
    UnitLabIedModelReportControlKind kind,
    char*** names,
    size_t* count,
    char* error,
    size_t error_size)
{
    if (names != NULL) {
        *names = NULL;
    }
    if (count != NULL) {
        *count = 0U;
    }
    if (plan == NULL || logical_device_inst == NULL || logical_node_name == NULL || names == NULL || count == NULL) {
        set_error(error, error_size, "INVALID_ARGUMENT: plan, logical device, logical node, names, and count are required.");
        return 0;
    }
    for (size_t index = 0U; index < plan->report_count; index++) {
        const UnitLabIedModelReportControl* report = &plan->reports[index];
        if (strcmp(report->logical_device_inst, logical_device_inst) != 0 || strcmp(report->logical_node_name, logical_node_name) != 0) {
            continue;
        }
        if ((kind == UNITLAB_IED_MODEL_REPORT_CONTROL_KIND_BUFFERED && !report->is_buffered)
            || (kind == UNITLAB_IED_MODEL_REPORT_CONTROL_KIND_UNBUFFERED && report->is_buffered)) {
            continue;
        }
        if (!append_metadata_name(names, count, report->name)) {
            unitlab_free_ied_model_name_list(*names, *count);
            *names = NULL;
            *count = 0U;
            set_error(error, error_size, "OUT_OF_MEMORY: cannot collect ReportControl names.");
            return 0;
        }
    }
    return 1;
}

const UnitLabIedModelReportControl* unitlab_find_ied_model_report_control(
    const UnitLabIedModelPlan* plan,
    const char* logical_device_inst,
    const char* logical_node_name,
    const char* report_name)
{
    if (plan == NULL || logical_device_inst == NULL || logical_node_name == NULL || report_name == NULL) {
        return NULL;
    }
    for (size_t index = 0U; index < plan->report_count; index++) {
        const UnitLabIedModelReportControl* report = &plan->reports[index];
        if (
            strcmp(report->logical_device_inst, logical_device_inst) == 0
            && strcmp(report->logical_node_name, logical_node_name) == 0
            && strcmp(report->name, report_name) == 0
        ) {
            return report;
        }
    }
    return NULL;
}

const UnitLabIedModelDataSet* unitlab_find_ied_model_data_set(
    const UnitLabIedModelPlan* plan,
    const char* logical_device_inst,
    const char* logical_node_name,
    const char* data_set_name)
{
    if (plan == NULL || logical_device_inst == NULL || logical_node_name == NULL || data_set_name == NULL) {
        return NULL;
    }
    for (size_t index = 0U; index < plan->data_set_count; index++) {
        const UnitLabIedModelDataSet* data_set = &plan->data_sets[index];
        if (
            strcmp(data_set->logical_device_inst, logical_device_inst) == 0
            && strcmp(data_set->logical_node_name, logical_node_name) == 0
            && strcmp(data_set->name, data_set_name) == 0
        ) {
            return data_set;
        }
    }
    return NULL;
}

const UnitLabIedModelNamespaceAttribute* unitlab_find_ied_model_namespace_attribute(
    const UnitLabIedModelPlan* plan,
    const char* object_reference)
{
    if (plan == NULL || object_reference == NULL) {
        return NULL;
    }
    for (size_t index = 0U; index < plan->namespace_attribute_count; index++) {
        const UnitLabIedModelNamespaceAttribute* attribute = &plan->namespace_attributes[index];
        if (strcmp(attribute->object_reference, object_reference) == 0) {
            return attribute;
        }
    }
    return NULL;
}

void unitlab_free_ied_model_name_list(char** names, size_t count)
{
    if (names == NULL) {
        return;
    }
    for (size_t index = 0U; index < count; index++) {
        free(names[index]);
    }
    free(names);
}

