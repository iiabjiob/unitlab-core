#include "app/planner/unitlab_verification_target.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void set_error(char* error, size_t error_size, const char* message)
{
    if (error == NULL || error_size == 0U) {
        return;
    }
    snprintf(error, error_size, "%s", message);
}

static int compare_size_t(const void* left, const void* right)
{
    const size_t left_value = *(const size_t*)left;
    const size_t right_value = *(const size_t*)right;
    if (left_value < right_value) {
        return -1;
    }
    if (left_value > right_value) {
        return 1;
    }
    return 0;
}

static void copy_c_string(char* destination, size_t destination_size, const char* source)
{
    if (destination == NULL || destination_size == 0U) {
        return;
    }
    if (source == NULL) {
        source = "";
    }
    snprintf(destination, destination_size, "%s", source);
}

static void copy_reference_without_fc(char* destination, size_t destination_size, const char* source)
{
    if (destination == NULL || destination_size == 0U) {
        return;
    }
    destination[0] = '\0';
    if (source == NULL || source[0] == '\0') {
        return;
    }
    const char* fc_suffix = strchr(source, '[');
    if (fc_suffix == NULL) {
        copy_c_string(destination, destination_size, source);
        return;
    }
    size_t length = (size_t)(fc_suffix - source);
    if (length >= destination_size) {
        length = destination_size - 1U;
    }
    memcpy(destination, source, length);
    destination[length] = '\0';
}

static void format_signal_path(const UnitLabIedModelSignal* signal, char* destination, size_t destination_size)
{
    if (destination == NULL || destination_size == 0U) {
        return;
    }
    destination[0] = '\0';
    if (signal == NULL) {
        return;
    }
    if (
        signal->logical_device_inst[0] != '\0'
        && signal->logical_node_name[0] != '\0'
        && signal->object_reference[0] != '\0'
    ) {
        snprintf(
            destination,
            destination_size,
            "%s/%s.%s",
            signal->logical_device_inst,
            signal->logical_node_name,
            signal->object_reference);
        return;
    }
    if (signal->reference[0] != '\0') {
        copy_reference_without_fc(destination, destination_size, signal->reference);
        return;
    }
    if (signal->object_reference[0] != '\0') {
        copy_c_string(destination, destination_size, signal->object_reference);
    }
}

static void format_endpoint_identity(
    const UnitLabIedFixtureModel* fixture,
    const char* endpoint_id,
    char* endpoint_id_output,
    size_t endpoint_id_output_size,
    char* endpoint_label_output,
    size_t endpoint_label_output_size)
{
    if (endpoint_id_output == NULL || endpoint_id_output_size == 0U || endpoint_label_output == NULL || endpoint_label_output_size == 0U) {
        return;
    }
    endpoint_id_output[0] = '\0';
    endpoint_label_output[0] = '\0';
    if (endpoint_id != NULL && endpoint_id[0] != '\0') {
        copy_c_string(endpoint_id_output, endpoint_id_output_size, endpoint_id);
        copy_c_string(endpoint_label_output, endpoint_label_output_size, endpoint_id);
        return;
    }
    if (fixture != NULL && fixture->ied_name[0] != '\0') {
        if (fixture->access_point_name[0] != '\0') {
            snprintf(
                endpoint_id_output,
                endpoint_id_output_size,
                "%s@%s",
                fixture->ied_name,
                fixture->access_point_name);
        } else {
            copy_c_string(endpoint_id_output, endpoint_id_output_size, fixture->ied_name);
        }
        copy_c_string(endpoint_label_output, endpoint_label_output_size, endpoint_id_output);
    }
}

static int collect_target_indexes(
    const UnitLabIedModelPlan* plan,
    const size_t* selected_signal_indexes,
    size_t selected_signal_count,
    size_t** indexes_out,
    size_t* count_out,
    char* error,
    size_t error_size)
{
    if (indexes_out == NULL || count_out == NULL) {
        return 0;
    }
    *indexes_out = NULL;
    *count_out = 0U;
    if (selected_signal_count == 0U) {
        return 1;
    }
    if (selected_signal_indexes == NULL) {
        set_error(error, error_size, "UNITLAB_VERIFICATION_TARGET_SELECTION_MISSING: selected signal indexes are required.");
        return 0;
    }
    size_t* indexes = (size_t*)malloc(selected_signal_count * sizeof(size_t));
    if (indexes == NULL) {
        set_error(error, error_size, "OUT_OF_MEMORY: cannot collect verification target indexes.");
        return 0;
    }
    for (size_t index = 0U; index < selected_signal_count; index++) {
        indexes[index] = selected_signal_indexes[index];
        if (indexes[index] >= plan->signal_count) {
            free(indexes);
            set_error(error, error_size, "UNITLAB_VERIFICATION_TARGET_INDEX_OUT_OF_RANGE: selected signal index is out of range.");
            return 0;
        }
    }
    qsort(indexes, selected_signal_count, sizeof(size_t), compare_size_t);
    *indexes_out = indexes;
    *count_out = selected_signal_count;
    return 1;
}

int unitlab_collect_ied_verification_targets(
    const UnitLabIedFixtureModel* fixture,
    const UnitLabIedModelPlan* plan,
    const char* endpoint_id,
    const size_t* selected_signal_indexes,
    size_t selected_signal_count,
    uint32_t timeout_ms,
    uint32_t window_ms,
    UnitLabIedVerificationTarget** targets,
    size_t* count,
    char* error,
    size_t error_size)
{
    if (targets == NULL || count == NULL) {
        return 0;
    }
    *targets = NULL;
    *count = 0U;
    if (fixture == NULL || plan == NULL) {
        set_error(error, error_size, "UNITLAB_VERIFICATION_TARGETS_INVALID_INPUT: fixture and plan are required.");
        return 0;
    }
    if (plan->signal_count == 0U) {
        return 1;
    }

    size_t* indexes = NULL;
    size_t index_count = 0U;
    if (!collect_target_indexes(plan, selected_signal_indexes, selected_signal_count, &indexes, &index_count, error, error_size)) {
        return 0;
    }

    UnitLabIedVerificationTarget* collected = (UnitLabIedVerificationTarget*)calloc(index_count, sizeof(UnitLabIedVerificationTarget));
    if (index_count > 0U && collected == NULL) {
        free(indexes);
        set_error(error, error_size, "OUT_OF_MEMORY: cannot collect verification targets.");
        return 0;
    }

    char endpoint_id_buffer[160];
    char endpoint_label_buffer[192];
    format_endpoint_identity(fixture, endpoint_id, endpoint_id_buffer, sizeof(endpoint_id_buffer), endpoint_label_buffer, sizeof(endpoint_label_buffer));

    for (size_t output_index = 0U; output_index < index_count; output_index++) {
        const size_t signal_index = indexes[output_index];
        const UnitLabIedModelSignal* signal = &plan->signals[signal_index];
        UnitLabIedVerificationTarget* target = &collected[output_index];
        target->source_signal_index = signal_index;
        target->source_data_set_index = signal->data_set_index;
        target->source_member_index = signal->member_index;
        copy_c_string(target->ied_name, sizeof(target->ied_name), fixture->ied_name);
        copy_c_string(target->access_point_name, sizeof(target->access_point_name), fixture->access_point_name);
        copy_c_string(target->endpoint_id, sizeof(target->endpoint_id), endpoint_id_buffer);
        copy_c_string(target->endpoint_label, sizeof(target->endpoint_label), endpoint_label_buffer);
        copy_c_string(target->signal_reference, sizeof(target->signal_reference), signal->reference);
        format_signal_path(signal, target->signal_path, sizeof(target->signal_path));
        copy_c_string(target->logical_device_inst, sizeof(target->logical_device_inst), signal->logical_device_inst);
        copy_c_string(target->logical_node_name, sizeof(target->logical_node_name), signal->logical_node_name);
        if (signal->data_set_index < plan->data_set_count && plan->data_sets != NULL) {
            copy_c_string(
                target->data_set_reference,
                sizeof(target->data_set_reference),
                plan->data_sets[signal->data_set_index].reference);
        }
        copy_c_string(
            target->expected_feedback_path,
            sizeof(target->expected_feedback_path),
            signal->data_set_entry_variable);
        if (target->expected_feedback_path[0] == '\0') {
            copy_c_string(target->expected_feedback_path, sizeof(target->expected_feedback_path), target->signal_path);
        }
        target->timeout_ms = timeout_ms;
        target->window_ms = window_ms;
    }

    free(indexes);
    *targets = collected;
    *count = index_count;
    return 1;
}

void unitlab_free_ied_verification_targets(UnitLabIedVerificationTarget* targets)
{
    free(targets);
}
