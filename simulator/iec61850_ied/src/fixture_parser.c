#include "fixture_parser.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct JsonRange {
    const char* start;
    const char* end;
} JsonRange;

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

static const char* skip_ws(const char* cursor, const char* end)
{
    while (cursor < end && (*cursor == ' ' || *cursor == '\n' || *cursor == '\r' || *cursor == '\t')) {
        cursor++;
    }
    return cursor;
}

static const char* find_key(JsonRange range, const char* key)
{
    char pattern[96];
    int written = snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    if (written <= 0 || (size_t)written >= sizeof(pattern)) {
        return NULL;
    }

    const char* cursor = range.start;
    while (cursor < range.end) {
        const char* found = strstr(cursor, pattern);
        if (found == NULL || found >= range.end) {
            return NULL;
        }
        return found;
    }
    return NULL;
}

static const char* parse_json_string(const char* cursor, const char* end, char* output, size_t output_size)
{
    if (cursor >= end || *cursor != '"' || output == NULL || output_size == 0U) {
        return NULL;
    }
    cursor++;

    size_t used = 0U;
    while (cursor < end) {
        char current = *cursor++;
        if (current == '"') {
            if (output_size > 0U) {
                output[used < output_size ? used : output_size - 1U] = '\0';
            }
            return cursor;
        }
        if (current == '\\') {
            if (cursor >= end) {
                return NULL;
            }
            current = *cursor++;
        }
        if (used + 1U >= output_size) {
            return NULL;
        }
        output[used] = current;
        used++;
    }
    return NULL;
}

static const char* parse_string_after_key(JsonRange range, const char* key, char* output, size_t output_size)
{
    const char* key_pos = find_key(range, key);
    if (key_pos == NULL) {
        return NULL;
    }

    const char* cursor = key_pos + strlen(key) + 2U;
    cursor = skip_ws(cursor, range.end);
    if (cursor >= range.end || *cursor != ':') {
        return NULL;
    }
    cursor++;
    cursor = skip_ws(cursor, range.end);
    return parse_json_string(cursor, range.end, output, output_size);
}

static int parse_nullable_string_after_key(JsonRange range, const char* key, char* output, size_t output_size)
{
    if (output == NULL || output_size == 0U) {
        return 0;
    }
    output[0] = '\0';

    const char* key_pos = find_key(range, key);
    if (key_pos == NULL) {
        return 1;
    }

    const char* cursor = key_pos + strlen(key) + 2U;
    cursor = skip_ws(cursor, range.end);
    if (cursor >= range.end || *cursor != ':') {
        return 0;
    }
    cursor++;
    cursor = skip_ws(cursor, range.end);
    if (cursor + 4 <= range.end && strncmp(cursor, "null", 4U) == 0) {
        return 1;
    }
    return parse_json_string(cursor, range.end, output, output_size) != NULL;
}

static int parse_bool_token(const char* cursor, const char* end, int* value)
{
    if (cursor + 4 <= end && strncmp(cursor, "true", 4U) == 0) {
        *value = 1;
        return 1;
    }
    if (cursor + 5 <= end && strncmp(cursor, "false", 5U) == 0) {
        *value = 0;
        return 1;
    }
    return 0;
}

static int parse_optional_bool_after_key(JsonRange range, const char* key, int* known, int* value)
{
    *known = 0;
    *value = 0;

    const char* key_pos = find_key(range, key);
    if (key_pos == NULL) {
        return 1;
    }

    const char* cursor = key_pos + strlen(key) + 2U;
    cursor = skip_ws(cursor, range.end);
    if (cursor >= range.end || *cursor != ':') {
        return 0;
    }
    cursor++;
    cursor = skip_ws(cursor, range.end);
    if (cursor + 4 <= range.end && strncmp(cursor, "null", 4U) == 0) {
        return 1;
    }
    if (!parse_bool_token(cursor, range.end, value)) {
        return 0;
    }
    *known = 1;
    return 1;
}

static int parse_optional_bool_field(JsonRange range, const char* key, UnitLabIedFixtureOptionalBool* field)
{
    return parse_optional_bool_after_key(range, key, &field->known, &field->value);
}

static int parse_long_token(const char* cursor, const char* end, long* value)
{
    char* parsed_end = NULL;
    long parsed = strtol(cursor, &parsed_end, 10);
    if (parsed_end == cursor || parsed_end == NULL || parsed_end > end) {
        return 0;
    }
    *value = parsed;
    return 1;
}

static int parse_size_after_key(JsonRange range, const char* key, size_t* value)
{
    const char* key_pos = find_key(range, key);
    if (key_pos == NULL) {
        return 0;
    }

    const char* cursor = key_pos + strlen(key) + 2U;
    cursor = skip_ws(cursor, range.end);
    if (cursor >= range.end || *cursor != ':') {
        return 0;
    }
    cursor++;
    cursor = skip_ws(cursor, range.end);

    long parsed = 0;
    if (!parse_long_token(cursor, range.end, &parsed) || parsed < 0) {
        return 0;
    }
    *value = (size_t)parsed;
    return 1;
}

static int parse_optional_int_after_key(JsonRange range, const char* key, int* known, int* value)
{
    *known = 0;
    *value = 0;

    const char* key_pos = find_key(range, key);
    if (key_pos == NULL) {
        return 1;
    }

    const char* cursor = key_pos + strlen(key) + 2U;
    cursor = skip_ws(cursor, range.end);
    if (cursor >= range.end || *cursor != ':') {
        return 0;
    }
    cursor++;
    cursor = skip_ws(cursor, range.end);
    if (cursor + 4 <= range.end && strncmp(cursor, "null", 4U) == 0) {
        return 1;
    }

    long parsed = 0;
    if (!parse_long_token(cursor, range.end, &parsed) || parsed < 0 || parsed > 2147483647L) {
        return 0;
    }
    *known = 1;
    *value = (int)parsed;
    return 1;
}

static int parse_raw_value_after_key(JsonRange range, const char* key, char* output, size_t output_size)
{
    if (output == NULL || output_size == 0U) {
        return 0;
    }
    output[0] = '\0';

    const char* key_pos = find_key(range, key);
    if (key_pos == NULL) {
        return 0;
    }

    const char* cursor = key_pos + strlen(key) + 2U;
    cursor = skip_ws(cursor, range.end);
    if (cursor >= range.end || *cursor != ':') {
        return 0;
    }
    cursor++;
    cursor = skip_ws(cursor, range.end);

    if (cursor < range.end && *cursor == '"') {
        return parse_json_string(cursor, range.end, output, output_size) != NULL;
    }

    const char* value_start = cursor;
    while (cursor < range.end && *cursor != ',' && *cursor != '}' && *cursor != ']') {
        cursor++;
    }
    const char* value_end = cursor;
    while (value_end > value_start && (*(value_end - 1) == ' ' || *(value_end - 1) == '\n' || *(value_end - 1) == '\r' || *(value_end - 1) == '\t')) {
        value_end--;
    }
    size_t length = (size_t)(value_end - value_start);
    if (length == 0U || length >= output_size) {
        return 0;
    }
    memcpy(output, value_start, length);
    output[length] = '\0';
    return 1;
}

static const char* find_matching(const char* open_pos, const char* end, char open_char, char close_char)
{
    int depth = 0;
    int in_string = 0;
    int escaped = 0;

    for (const char* cursor = open_pos; cursor < end; cursor++) {
        char current = *cursor;
        if (in_string) {
            if (escaped) {
                escaped = 0;
            }
            else if (current == '\\') {
                escaped = 1;
            }
            else if (current == '"') {
                in_string = 0;
            }
            continue;
        }

        if (current == '"') {
            in_string = 1;
            continue;
        }
        if (current == open_char) {
            depth++;
            continue;
        }
        if (current == close_char) {
            depth--;
            if (depth == 0) {
                return cursor + 1;
            }
        }
    }
    return NULL;
}

static int extract_object_after_key(JsonRange range, const char* key, JsonRange* object_range)
{
    const char* key_pos = find_key(range, key);
    if (key_pos == NULL) {
        return 0;
    }

    const char* cursor = key_pos + strlen(key) + 2U;
    cursor = skip_ws(cursor, range.end);
    if (cursor >= range.end || *cursor != ':') {
        return 0;
    }
    cursor++;
    cursor = skip_ws(cursor, range.end);
    if (cursor >= range.end || *cursor != '{') {
        return 0;
    }

    const char* object_end = find_matching(cursor, range.end, '{', '}');
    if (object_end == NULL) {
        return 0;
    }
    object_range->start = cursor;
    object_range->end = object_end;
    return 1;
}

static int extract_array_after_key(JsonRange range, const char* key, JsonRange* array_range)
{
    const char* key_pos = find_key(range, key);
    if (key_pos == NULL) {
        return 0;
    }

    const char* cursor = key_pos + strlen(key) + 2U;
    cursor = skip_ws(cursor, range.end);
    if (cursor >= range.end || *cursor != ':') {
        return 0;
    }
    cursor++;
    cursor = skip_ws(cursor, range.end);
    if (cursor >= range.end || *cursor != '[') {
        return 0;
    }

    const char* array_end = find_matching(cursor, range.end, '[', ']');
    if (array_end == NULL) {
        return 0;
    }

    array_range->start = cursor;
    array_range->end = array_end;
    return 1;
}

static int next_top_level_object(JsonRange array_range, const char** cursor, JsonRange* object_range)
{
    const char* limit = array_range.end > array_range.start ? array_range.end - 1 : array_range.end;
    const char* pos = *cursor == NULL ? array_range.start + 1 : *cursor;
    pos = skip_ws(pos, limit);
    if (pos < limit && *pos == ',') {
        pos++;
        pos = skip_ws(pos, limit);
    }
    if (pos >= limit || *pos == ']') {
        return 0;
    }
    if (*pos != '{') {
        return -1;
    }
    const char* object_end = find_matching(pos, array_range.end, '{', '}');
    if (object_end == NULL || object_end > array_range.end) {
        return -1;
    }
    object_range->start = pos;
    object_range->end = object_end;
    *cursor = object_end;
    return 1;
}

static size_t count_top_level_objects(JsonRange array_range)
{
    size_t count = 0U;
    int in_string = 0;
    int escaped = 0;
    int object_depth = 0;
    int array_depth = 0;

    for (const char* cursor = array_range.start + 1; cursor + 1 <= array_range.end; cursor++) {
        char current = *cursor;
        if (in_string) {
            if (escaped) {
                escaped = 0;
            }
            else if (current == '\\') {
                escaped = 1;
            }
            else if (current == '"') {
                in_string = 0;
            }
            continue;
        }

        if (current == '"') {
            in_string = 1;
            continue;
        }
        if (current == '[') {
            array_depth++;
            continue;
        }
        if (current == ']') {
            if (array_depth > 0) {
                array_depth--;
            }
            continue;
        }
        if (current == '{') {
            if (array_depth == 0 && object_depth == 0) {
                count++;
            }
            object_depth++;
            continue;
        }
        if (current == '}' && object_depth > 0) {
            object_depth--;
        }
    }

    return count;
}

static int validate_schema(JsonRange root, char* error, size_t error_size)
{
    char schema[128];
    if (parse_string_after_key(root, "schema", schema, sizeof(schema)) == NULL) {
        set_error(error, error_size, "FIXTURE_SCHEMA_MISSING: schema field is required.");
        return 0;
    }
    if (strcmp(schema, UNITLAB_IED_SIM_SCHEMA) != 0) {
        set_error(error, error_size, "FIXTURE_SCHEMA_MISMATCH: expected schema %s.", UNITLAB_IED_SIM_SCHEMA);
        return 0;
    }
    return 1;
}

static int parse_trigger_options(
    JsonRange report_range,
    UnitLabIedFixtureTriggerOptions* options,
    char* error,
    size_t error_size)
{
    JsonRange options_range;
    if (!extract_object_after_key(report_range, "triggerOptions", &options_range)) {
        set_error(error, error_size, "FIXTURE_REPORT_TRGOPS_MISSING: ReportControl requires triggerOptions object.");
        return 0;
    }
    if (!parse_optional_bool_field(options_range, "dataChange", &options->data_change)) {
        set_error(error, error_size, "FIXTURE_REPORT_TRGOPS_INVALID: dataChange must be a boolean or null.");
        return 0;
    }
    if (!parse_optional_bool_field(options_range, "qualityChange", &options->quality_change)) {
        set_error(error, error_size, "FIXTURE_REPORT_TRGOPS_INVALID: qualityChange must be a boolean or null.");
        return 0;
    }
    if (!parse_optional_bool_field(options_range, "dataUpdate", &options->data_update)) {
        set_error(error, error_size, "FIXTURE_REPORT_TRGOPS_INVALID: dataUpdate must be a boolean or null.");
        return 0;
    }
    if (!parse_optional_bool_field(options_range, "periodic", &options->periodic)) {
        set_error(error, error_size, "FIXTURE_REPORT_TRGOPS_INVALID: periodic must be a boolean or null.");
        return 0;
    }
    if (!parse_optional_bool_field(options_range, "generalInterrogation", &options->general_interrogation)) {
        set_error(error, error_size, "FIXTURE_REPORT_TRGOPS_INVALID: generalInterrogation must be a boolean or null.");
        return 0;
    }
    return 1;
}

static int parse_optional_fields(
    JsonRange report_range,
    UnitLabIedFixtureOptionalFields* fields,
    char* error,
    size_t error_size)
{
    JsonRange fields_range;
    if (!extract_object_after_key(report_range, "optionalFields", &fields_range)) {
        set_error(error, error_size, "FIXTURE_REPORT_OPTFIELDS_MISSING: ReportControl requires optionalFields object.");
        return 0;
    }
    if (!parse_optional_bool_field(fields_range, "sequenceNumber", &fields->sequence_number)) {
        set_error(error, error_size, "FIXTURE_REPORT_OPTFIELDS_INVALID: sequenceNumber must be a boolean or null.");
        return 0;
    }
    if (!parse_optional_bool_field(fields_range, "timestamp", &fields->timestamp)) {
        set_error(error, error_size, "FIXTURE_REPORT_OPTFIELDS_INVALID: timestamp must be a boolean or null.");
        return 0;
    }
    if (!parse_optional_bool_field(fields_range, "reasonCode", &fields->reason_code)) {
        set_error(error, error_size, "FIXTURE_REPORT_OPTFIELDS_INVALID: reasonCode must be a boolean or null.");
        return 0;
    }
    if (!parse_optional_bool_field(fields_range, "dataSetName", &fields->data_set_name)) {
        set_error(error, error_size, "FIXTURE_REPORT_OPTFIELDS_INVALID: dataSetName must be a boolean or null.");
        return 0;
    }
    if (!parse_optional_bool_field(fields_range, "dataReference", &fields->data_reference)) {
        set_error(error, error_size, "FIXTURE_REPORT_OPTFIELDS_INVALID: dataReference must be a boolean or null.");
        return 0;
    }
    if (!parse_optional_bool_field(fields_range, "entryId", &fields->entry_id)) {
        set_error(error, error_size, "FIXTURE_REPORT_OPTFIELDS_INVALID: entryId must be a boolean or null.");
        return 0;
    }
    if (!parse_optional_bool_field(fields_range, "configRevision", &fields->config_revision)) {
        set_error(error, error_size, "FIXTURE_REPORT_OPTFIELDS_INVALID: configRevision must be a boolean or null.");
        return 0;
    }
    if (!parse_optional_bool_field(fields_range, "bufferOverflow", &fields->buffer_overflow)) {
        set_error(error, error_size, "FIXTURE_REPORT_OPTFIELDS_INVALID: bufferOverflow must be a boolean or null.");
        return 0;
    }
    return 1;
}

static int parse_signal(JsonRange signal_range, UnitLabIedFixtureSignal* signal, char* error, size_t error_size)
{
    if (!parse_size_after_key(signal_range, "dataSetIndex", &signal->data_set_index)) {
        set_error(error, error_size, "FIXTURE_SIGNAL_INDEX_INVALID: DataSet member requires dataSetIndex.");
        return 0;
    }
    if (parse_string_after_key(signal_range, "reference", signal->reference, sizeof(signal->reference)) == NULL) {
        set_error(error, error_size, "FIXTURE_SIGNAL_REFERENCE_INVALID: DataSet member requires reference.");
        return 0;
    }
    if (parse_string_after_key(signal_range, "kind", signal->kind, sizeof(signal->kind)) == NULL) {
        set_error(error, error_size, "FIXTURE_SIGNAL_KIND_INVALID: DataSet member requires kind.");
        return 0;
    }
    if (!parse_nullable_string_after_key(signal_range, "fc", signal->fc, sizeof(signal->fc))) {
        set_error(error, error_size, "FIXTURE_SIGNAL_FC_INVALID: DataSet member fc must be a string or null.");
        return 0;
    }
    if (!parse_raw_value_after_key(signal_range, "initialValue", signal->initial_value, sizeof(signal->initial_value))) {
        set_error(error, error_size, "FIXTURE_SIGNAL_INITIAL_VALUE_INVALID: DataSet member requires initialValue.");
        return 0;
    }
    return 1;
}

static int parse_data_set(JsonRange data_set_range, UnitLabIedFixtureDataSet* data_set, char* error, size_t error_size)
{
    if (parse_string_after_key(data_set_range, "reference", data_set->reference, sizeof(data_set->reference)) == NULL) {
        set_error(error, error_size, "FIXTURE_DATASET_REFERENCE_INVALID: DataSet requires reference.");
        return 0;
    }

    JsonRange members_range;
    if (!extract_array_after_key(data_set_range, "members", &members_range)) {
        set_error(error, error_size, "FIXTURE_DATASET_MEMBERS_MISSING: DataSet requires members array.");
        return 0;
    }
    data_set->signal_count = count_top_level_objects(members_range);
    if (data_set->signal_count == 0U) {
        set_error(error, error_size, "FIXTURE_DATASET_MEMBERS_EMPTY: DataSet has no members.");
        return 0;
    }

    data_set->signals = (UnitLabIedFixtureSignal*)calloc(data_set->signal_count, sizeof(UnitLabIedFixtureSignal));
    if (data_set->signals == NULL) {
        set_error(error, error_size, "OUT_OF_MEMORY: cannot allocate DataSet members.");
        return 0;
    }

    const char* cursor = NULL;
    JsonRange member_range;
    for (size_t index = 0U; index < data_set->signal_count; index++) {
        int next = next_top_level_object(members_range, &cursor, &member_range);
        if (next <= 0) {
            set_error(error, error_size, "FIXTURE_DATASET_MEMBERS_INVALID: DataSet member array is malformed.");
            return 0;
        }
        if (!parse_signal(member_range, &data_set->signals[index], error, error_size)) {
            return 0;
        }
    }
    return 1;
}

static int parse_report(JsonRange report_range, UnitLabIedFixtureReport* report, char* error, size_t error_size)
{
    if (parse_string_after_key(report_range, "key", report->key, sizeof(report->key)) == NULL) {
        set_error(error, error_size, "FIXTURE_REPORT_KEY_INVALID: ReportControl requires key.");
        return 0;
    }
    if (parse_string_after_key(report_range, "logicalDeviceInst", report->logical_device_inst, sizeof(report->logical_device_inst)) == NULL) {
        set_error(error, error_size, "FIXTURE_REPORT_LD_INVALID: ReportControl requires logicalDeviceInst.");
        return 0;
    }
    if (parse_string_after_key(report_range, "logicalNodeName", report->logical_node_name, sizeof(report->logical_node_name)) == NULL) {
        set_error(error, error_size, "FIXTURE_REPORT_LN_INVALID: ReportControl requires logicalNodeName.");
        return 0;
    }
    if (parse_string_after_key(report_range, "reportControlName", report->report_control_name, sizeof(report->report_control_name)) == NULL) {
        set_error(error, error_size, "FIXTURE_REPORT_NAME_INVALID: ReportControl requires reportControlName.");
        return 0;
    }
    if (parse_string_after_key(report_range, "reportKind", report->report_kind, sizeof(report->report_kind)) == NULL) {
        set_error(error, error_size, "FIXTURE_REPORT_KIND_INVALID: ReportControl requires reportKind.");
        return 0;
    }
    if (!parse_nullable_string_after_key(report_range, "rptId", report->rpt_id, sizeof(report->rpt_id))) {
        set_error(error, error_size, "FIXTURE_REPORT_RPTID_INVALID: ReportControl rptId must be a string or null.");
        return 0;
    }
    if (parse_string_after_key(report_range, "dataSetRef", report->data_set_ref, sizeof(report->data_set_ref)) == NULL) {
        set_error(error, error_size, "FIXTURE_REPORT_DATASET_INVALID: ReportControl requires dataSetRef.");
        return 0;
    }
    if (!parse_nullable_string_after_key(report_range, "confRev", report->conf_rev, sizeof(report->conf_rev))) {
        set_error(error, error_size, "FIXTURE_REPORT_CONFREV_INVALID: ReportControl confRev must be a string or null.");
        return 0;
    }
    if (!parse_optional_bool_after_key(report_range, "indexed", &report->indexed_known, &report->indexed)) {
        set_error(error, error_size, "FIXTURE_REPORT_INDEXED_INVALID: ReportControl indexed must be a boolean or null.");
        return 0;
    }
    if (!parse_optional_int_after_key(report_range, "bufferTimeMs", &report->buffer_time_ms_known, &report->buffer_time_ms)) {
        set_error(error, error_size, "FIXTURE_REPORT_BUFTM_INVALID: ReportControl bufferTimeMs must be a non-negative integer or null.");
        return 0;
    }
    if (!parse_optional_int_after_key(report_range, "integrityPeriodMs", &report->integrity_period_ms_known, &report->integrity_period_ms)) {
        set_error(error, error_size, "FIXTURE_REPORT_INTGPD_INVALID: ReportControl integrityPeriodMs must be a non-negative integer or null.");
        return 0;
    }
    if (!parse_trigger_options(report_range, &report->trigger_options, error, error_size)) {
        return 0;
    }
    if (!parse_optional_fields(report_range, &report->optional_fields, error, error_size)) {
        return 0;
    }
    return 1;
}

static int parse_selected_device(JsonRange device_range, UnitLabIedFixtureModel* model, char* error, size_t error_size)
{
    if (parse_string_after_key(device_range, "accessPointName", model->access_point_name, sizeof(model->access_point_name)) == NULL) {
        set_error(error, error_size, "FIXTURE_ACCESS_POINT_MISSING: selected IED requires accessPointName.");
        return 0;
    }

    JsonRange data_sets_range;
    if (!extract_array_after_key(device_range, "dataSets", &data_sets_range)) {
        set_error(error, error_size, "FIXTURE_DATASETS_MISSING: selected IED requires dataSets array.");
        return 0;
    }
    JsonRange reports_range;
    if (!extract_array_after_key(device_range, "reports", &reports_range)) {
        set_error(error, error_size, "FIXTURE_REPORTS_MISSING: selected IED requires reports array.");
        return 0;
    }

    model->data_set_count = count_top_level_objects(data_sets_range);
    model->report_count = count_top_level_objects(reports_range);
    if (model->data_set_count == 0U) {
        set_error(error, error_size, "FIXTURE_DATASETS_EMPTY: selected IED has no DataSets.");
        return 0;
    }
    if (model->report_count == 0U) {
        set_error(error, error_size, "FIXTURE_REPORTS_EMPTY: selected IED has no ReportControls.");
        return 0;
    }

    model->data_sets = (UnitLabIedFixtureDataSet*)calloc(model->data_set_count, sizeof(UnitLabIedFixtureDataSet));
    if (model->data_sets == NULL) {
        set_error(error, error_size, "OUT_OF_MEMORY: cannot allocate DataSets.");
        return 0;
    }
    model->reports = (UnitLabIedFixtureReport*)calloc(model->report_count, sizeof(UnitLabIedFixtureReport));
    if (model->reports == NULL) {
        set_error(error, error_size, "OUT_OF_MEMORY: cannot allocate ReportControls.");
        return 0;
    }

    const char* data_set_cursor = NULL;
    JsonRange data_set_range;
    for (size_t index = 0U; index < model->data_set_count; index++) {
        int next = next_top_level_object(data_sets_range, &data_set_cursor, &data_set_range);
        if (next <= 0) {
            set_error(error, error_size, "FIXTURE_DATASETS_INVALID: DataSet array is malformed.");
            return 0;
        }
        if (!parse_data_set(data_set_range, &model->data_sets[index], error, error_size)) {
            return 0;
        }
        model->signal_count += model->data_sets[index].signal_count;
    }

    const char* report_cursor = NULL;
    JsonRange report_range;
    for (size_t index = 0U; index < model->report_count; index++) {
        int next = next_top_level_object(reports_range, &report_cursor, &report_range);
        if (next <= 0) {
            set_error(error, error_size, "FIXTURE_REPORTS_INVALID: ReportControl array is malformed.");
            return 0;
        }
        if (!parse_report(report_range, &model->reports[index], error, error_size)) {
            return 0;
        }
    }

    return 1;
}

int unitlab_parse_ied_fixture_summary(
    const char* fixture_text,
    const char* ied_name,
    UnitLabIedFixtureSummary* summary,
    char* error,
    size_t error_size)
{
    UnitLabIedFixtureModel model;
    if (!unitlab_parse_ied_fixture_model(fixture_text, ied_name, &model, error, error_size)) {
        return 0;
    }

    memset(summary, 0, sizeof(*summary));
    summary->device_count = model.device_count;
    summary->data_set_count = model.data_set_count;
    summary->report_count = model.report_count;
    summary->signal_count = model.signal_count;
    snprintf(summary->access_point_name, sizeof(summary->access_point_name), "%s", model.access_point_name);
    unitlab_free_ied_fixture_model(&model);
    return 1;
}

int unitlab_parse_ied_fixture_model(
    const char* fixture_text,
    const char* ied_name,
    UnitLabIedFixtureModel* model,
    char* error,
    size_t error_size)
{
    if (fixture_text == NULL || ied_name == NULL || model == NULL) {
        set_error(error, error_size, "INVALID_ARGUMENT: fixture text, IED name, and summary are required.");
        return 0;
    }

    memset(model, 0, sizeof(*model));
    JsonRange root = { fixture_text, fixture_text + strlen(fixture_text) };
    if (!validate_schema(root, error, error_size)) {
        return 0;
    }

    JsonRange devices_range;
    if (!extract_array_after_key(root, "devices", &devices_range)) {
        set_error(error, error_size, "FIXTURE_DEVICES_MISSING: devices array is required.");
        return 0;
    }
    model->device_count = count_top_level_objects(devices_range);
    if (model->device_count == 0U) {
        set_error(error, error_size, "FIXTURE_DEVICES_EMPTY: fixture has no devices.");
        return 0;
    }

    const char* cursor = NULL;
    JsonRange device_range;
    while (1) {
        int next = next_top_level_object(devices_range, &cursor, &device_range);
        if (next < 0) {
            set_error(error, error_size, "FIXTURE_DEVICES_INVALID: devices array is malformed.");
            unitlab_free_ied_fixture_model(model);
            return 0;
        }
        if (next == 0) {
            break;
        }
        char current_ied[128];
        if (parse_string_after_key(device_range, "iedName", current_ied, sizeof(current_ied)) == NULL) {
            set_error(error, error_size, "FIXTURE_IED_NAME_INVALID: iedName must be a JSON string.");
            unitlab_free_ied_fixture_model(model);
            return 0;
        }

        if (strcmp(current_ied, ied_name) == 0) {
            snprintf(model->ied_name, sizeof(model->ied_name), "%s", current_ied);
            if (!parse_selected_device(device_range, model, error, error_size)) {
                unitlab_free_ied_fixture_model(model);
                return 0;
            }
            return 1;
        }
    }

    unitlab_free_ied_fixture_model(model);
    set_error(error, error_size, "FIXTURE_IED_NOT_FOUND: IED \"%s\" is not present in fixture.", ied_name);
    return 0;
}

void unitlab_free_ied_fixture_model(UnitLabIedFixtureModel* model)
{
    if (model == NULL) {
        return;
    }
    if (model->data_sets != NULL) {
        for (size_t index = 0U; index < model->data_set_count; index++) {
            free(model->data_sets[index].signals);
        }
    }
    free(model->data_sets);
    free(model->reports);
    memset(model, 0, sizeof(*model));
}
