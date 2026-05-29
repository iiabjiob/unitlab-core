#include "fixture_parser.h"

#include <stdarg.h>
#include <stdio.h>
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
    if (cursor >= end || *cursor != '"') {
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
        if (output_size > 0U && used + 1U < output_size) {
            output[used] = current;
        }
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

static const char* find_object_start(const char* key_pos, const char* text_start)
{
    const char* cursor = key_pos;
    while (cursor > text_start) {
        cursor--;
        if (*cursor == '{') {
            return cursor;
        }
    }
    return NULL;
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

static size_t count_key_occurrences(JsonRange range, const char* key)
{
    char pattern[96];
    int written = snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    if (written <= 0 || (size_t)written >= sizeof(pattern)) {
        return 0U;
    }

    size_t count = 0U;
    const char* cursor = range.start;
    while (cursor < range.end) {
        const char* found = strstr(cursor, pattern);
        if (found == NULL || found >= range.end) {
            break;
        }
        count++;
        cursor = found + (size_t)written;
    }
    return count;
}

int unitlab_parse_ied_fixture_summary(
    const char* fixture_text,
    const char* ied_name,
    UnitLabIedFixtureSummary* summary,
    char* error,
    size_t error_size)
{
    if (fixture_text == NULL || ied_name == NULL || summary == NULL) {
        set_error(error, error_size, "INVALID_ARGUMENT: fixture text, IED name, and summary are required.");
        return 0;
    }

    memset(summary, 0, sizeof(*summary));
    JsonRange root = { fixture_text, fixture_text + strlen(fixture_text) };

    char schema[128];
    if (parse_string_after_key(root, "schema", schema, sizeof(schema)) == NULL) {
        set_error(error, error_size, "FIXTURE_SCHEMA_MISSING: schema field is required.");
        return 0;
    }
    if (strcmp(schema, UNITLAB_IED_SIM_SCHEMA) != 0) {
        set_error(error, error_size, "FIXTURE_SCHEMA_MISMATCH: expected schema %s.", UNITLAB_IED_SIM_SCHEMA);
        return 0;
    }

    const char* cursor = root.start;
    while (cursor < root.end) {
        JsonRange search_range = { cursor, root.end };
        const char* ied_key = find_key(search_range, "iedName");
        if (ied_key == NULL) {
            break;
        }

        char current_ied[128];
        const char* after_ied = parse_string_after_key((JsonRange) { ied_key, root.end }, "iedName", current_ied, sizeof(current_ied));
        if (after_ied == NULL) {
            set_error(error, error_size, "FIXTURE_IED_NAME_INVALID: iedName must be a JSON string.");
            return 0;
        }

        summary->device_count++;
        if (strcmp(current_ied, ied_name) == 0) {
            const char* device_start = find_object_start(ied_key, root.start);
            if (device_start == NULL) {
                set_error(error, error_size, "FIXTURE_DEVICE_INVALID: cannot locate selected IED object.");
                return 0;
            }
            const char* device_end = find_matching(device_start, root.end, '{', '}');
            if (device_end == NULL) {
                set_error(error, error_size, "FIXTURE_DEVICE_INVALID: selected IED object is not closed.");
                return 0;
            }

            JsonRange device_range = { device_start, device_end };
            if (parse_string_after_key(device_range, "accessPointName", summary->access_point_name, sizeof(summary->access_point_name)) == NULL) {
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

            summary->data_set_count = count_top_level_objects(data_sets_range);
            summary->report_count = count_top_level_objects(reports_range);
            summary->signal_count = count_key_occurrences(data_sets_range, "dataSetIndex");
            if (summary->data_set_count == 0U) {
                set_error(error, error_size, "FIXTURE_DATASETS_EMPTY: selected IED has no DataSets.");
                return 0;
            }
            if (summary->report_count == 0U) {
                set_error(error, error_size, "FIXTURE_REPORTS_EMPTY: selected IED has no ReportControls.");
                return 0;
            }
            return 1;
        }

        cursor = after_ied;
    }

    set_error(error, error_size, "FIXTURE_IED_NOT_FOUND: IED \"%s\" is not present in fixture.", ied_name);
    return 0;
}
