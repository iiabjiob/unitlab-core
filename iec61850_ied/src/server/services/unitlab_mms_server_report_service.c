#include "server/unitlab_mms_server_runtime_internal.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

/* Report service owns BRCB value encoding and unconfirmed InformationReport construction. */

static const char* const buffered_report_control_block_fields[] = {
    "RptID", "RptEna", "DatSet", "ConfRev", "OptFlds", "BufTm", "SqNum",
    "TrgOps", "IntgPd", "GI", "PurgeBuf", "EntryID", "TimeofEntry", "ResvTms"
};

static const char* const unbuffered_report_control_block_fields[] = {
    "RptID", "RptEna", "Resv", "DatSet", "ConfRev", "OptFlds", "BufTm",
    "SqNum", "TrgOps", "IntgPd", "GI"
};

const char* const* server_runtime_report_control_block_fields(size_t* field_count)
{
    if (field_count != NULL) {
        *field_count = sizeof(buffered_report_control_block_fields) / sizeof(buffered_report_control_block_fields[0]);
    }
    return buffered_report_control_block_fields;
}

static int server_runtime_encode_report_control_block_structure_field_value(
    const UnitLabMmsServerRuntime* server_runtime,
    const char* field_name,
    const char* report_id_reference,
    const char* data_set_reference,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic);

static uint32_t server_runtime_report_conf_rev(const UnitLabIedModelReportControl* report)
{
    return report != NULL && report->conf_rev_known ? report->conf_rev : 7U;
}

static uint32_t server_runtime_report_buffer_time_ms(const UnitLabIedModelReportControl* report)
{
    return report != NULL && report->buffer_time_ms_known ? report->buffer_time_ms : 100U;
}

static uint32_t server_runtime_report_integrity_period_ms(const UnitLabIedModelReportControl* report)
{
    return report != NULL && report->integrity_period_ms_known ? report->integrity_period_ms : 1000U;
}

static uint8_t server_runtime_report_optional_fields_mask(const UnitLabMmsServerRuntime* server_runtime, const UnitLabIedModelReportControl* report)
{
    if (server_runtime != NULL && server_runtime->brcb_optional_fields_mask_known != 0U) {
        return server_runtime->brcb_optional_fields_mask;
    }
    return report != NULL ? report->optional_fields_mask : 0xFFU;
}

static uint8_t server_runtime_report_trigger_options_mask(const UnitLabMmsServerRuntime* server_runtime, const UnitLabIedModelReportControl* report)
{
    if (server_runtime != NULL && server_runtime->brcb_trigger_options_mask_known != 0U) {
        return server_runtime->brcb_trigger_options_mask;
    }
    return report != NULL ? report->trigger_options_mask : (UNITLAB_IED_MODEL_TRG_OPT_DATA_CHANGED | UNITLAB_IED_MODEL_TRG_OPT_QUALITY_CHANGED | UNITLAB_IED_MODEL_TRG_OPT_GI);
}

static int server_runtime_report_optional_field_enabled(const UnitLabMmsServerRuntime* server_runtime, const UnitLabIedModelReportControl* report, uint8_t option)
{
    return (server_runtime_report_optional_fields_mask(server_runtime, report) & option) != 0U;
}

static void server_runtime_encode_report_optional_fields_bitstring(const UnitLabMmsServerRuntime* server_runtime, const UnitLabIedModelReportControl* report, uint8_t* buffer)
{
    uint8_t mask = server_runtime_report_optional_fields_mask(server_runtime, report);
    uint8_t wire_mask = 0U;

    if ((mask & UNITLAB_IED_MODEL_RPT_OPT_SEQ_NUM) != 0U) {
        wire_mask |= 0x40U;
    }
    if ((mask & UNITLAB_IED_MODEL_RPT_OPT_TIME_STAMP) != 0U) {
        wire_mask |= 0x20U;
    }
    if ((mask & UNITLAB_IED_MODEL_RPT_OPT_REASON_FOR_INCLUSION) != 0U) {
        wire_mask |= 0x10U;
    }
    if ((mask & UNITLAB_IED_MODEL_RPT_OPT_DATA_SET) != 0U) {
        wire_mask |= 0x08U;
    }
    if ((mask & UNITLAB_IED_MODEL_RPT_OPT_DATA_REFERENCE) != 0U) {
        wire_mask |= 0x04U;
    }
    if ((mask & UNITLAB_IED_MODEL_RPT_OPT_BUFFER_OVERFLOW) != 0U) {
        wire_mask |= 0x02U;
    }
    if ((mask & UNITLAB_IED_MODEL_RPT_OPT_ENTRY_ID) != 0U) {
        wire_mask |= 0x01U;
    }

    buffer[0] = 0x06U;
    buffer[1] = wire_mask;
    buffer[2] = (uint8_t)((mask & UNITLAB_IED_MODEL_RPT_OPT_CONF_REV) != 0U ? 0x80U : 0x00U);
}

static void server_runtime_encode_report_trigger_options_bitstring(const UnitLabMmsServerRuntime* server_runtime, const UnitLabIedModelReportControl* report, uint8_t* buffer)
{
    uint8_t mask = server_runtime_report_trigger_options_mask(server_runtime, report);
    uint8_t wire_mask = 0U;

    if ((mask & UNITLAB_IED_MODEL_TRG_OPT_DATA_CHANGED) != 0U) {
        wire_mask |= 0x40U;
    }
    if ((mask & UNITLAB_IED_MODEL_TRG_OPT_QUALITY_CHANGED) != 0U) {
        wire_mask |= 0x20U;
    }
    if ((mask & UNITLAB_IED_MODEL_TRG_OPT_DATA_UPDATE) != 0U) {
        wire_mask |= 0x10U;
    }
    if ((mask & UNITLAB_IED_MODEL_TRG_OPT_INTEGRITY) != 0U) {
        wire_mask |= 0x08U;
    }
    if ((mask & UNITLAB_IED_MODEL_TRG_OPT_GI) != 0U) {
        wire_mask |= 0x04U;
    }

    buffer[0] = 0x02U;
    buffer[1] = wire_mask;
}

static void server_runtime_format_report_id_reference(
    const UnitLabMmsServerRuntime* server_runtime,
    const UnitLabIedModelReportControl* report,
    char* buffer,
    size_t buffer_length)
{
    const char* domain_name = server_runtime_advertised_domain_name(server_runtime);

    if (buffer == NULL || buffer_length == 0U) {
        return;
    }
    if (report != NULL && report->rpt_id[0] != '\0') {
        snprintf(buffer, buffer_length, "%s", report->rpt_id);
        return;
    }
    snprintf(buffer, buffer_length, "%s/LLN0.BR.Events", domain_name);
}

static void server_runtime_format_dataset_reference(
    const UnitLabMmsServerRuntime* server_runtime,
    const UnitLabIedModelReportControl* report,
    const UnitLabIedModelDataSet* data_set,
    char* buffer,
    size_t buffer_length)
{
    const char* domain_name = server_runtime_advertised_domain_name(server_runtime);

    if (buffer == NULL || buffer_length == 0U) {
        return;
    }
    if (
        report != NULL
        && report->data_set_ref[0] != '\0'
        && data_set != NULL
        && data_set->logical_device_inst[0] != '\0'
        && data_set->logical_node_name[0] != '\0'
        && data_set->name[0] != '\0'
    ) {
        const char* first_slash = strchr(report->data_set_ref, '/');
        if (first_slash != NULL && first_slash != report->data_set_ref) {
            size_t ied_name_length = (size_t)(first_slash - report->data_set_ref);
            if (strncmp(data_set->logical_device_inst, report->data_set_ref, ied_name_length) == 0) {
                snprintf(
                    buffer,
                    buffer_length,
                    "%s/%s$%s",
                    data_set->logical_device_inst,
                    data_set->logical_node_name,
                    data_set->name);
            }
            else {
                snprintf(
                    buffer,
                    buffer_length,
                    "%.*s%s/%s$%s",
                    (int)ied_name_length,
                    report->data_set_ref,
                    data_set->logical_device_inst,
                    data_set->logical_node_name,
                    data_set->name);
            }
            return;
        }
    }
    if (data_set != NULL && data_set->logical_device_inst[0] != '\0' && data_set->logical_node_name[0] != '\0' && data_set->name[0] != '\0') {
        snprintf(buffer, buffer_length, "%s/%s$%s", data_set->logical_device_inst, data_set->logical_node_name, data_set->name);
        return;
    }
    snprintf(buffer, buffer_length, "%s/LLN0$dsEvents", domain_name);
}

void server_runtime_format_report_control_references(
    const UnitLabMmsServerRuntime* server_runtime,
    char* report_id_reference,
    size_t report_id_reference_size,
    char* data_set_reference,
    size_t data_set_reference_size)
{
    const UnitLabIedModelReportControl* report = server_runtime_active_model_report_control(server_runtime);

    const UnitLabIedModelDataSet* data_set = server_runtime_model_report_data_set(server_runtime, report);

    server_runtime_format_report_id_reference(server_runtime, report, report_id_reference, report_id_reference_size);
    server_runtime_format_dataset_reference(server_runtime, report, data_set, data_set_reference, data_set_reference_size);
}

static int server_runtime_encode_unsigned_value(uint32_t value, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic)
{
    uint8_t value_bytes[4U];
    size_t start = 0U;
    size_t length = sizeof(value_bytes);

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (buffer == NULL || encoded_length == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Unsigned value encoding requires a buffer and encoded_length.");
        return 0;
    }

    value_bytes[0] = (uint8_t)((value >> 24U) & 0xFFU);
    value_bytes[1] = (uint8_t)((value >> 16U) & 0xFFU);
    value_bytes[2] = (uint8_t)((value >> 8U) & 0xFFU);
    value_bytes[3] = (uint8_t)(value & 0xFFU);
    while (start + 1U < sizeof(value_bytes) && value_bytes[start] == 0U) {
        start++;
        length--;
    }
    if (length > buffer_length) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Unsigned value encoding buffer is too small.");
        return 0;
    }
    memcpy(buffer, &value_bytes[start], length);
    *encoded_length = length;
    return 1;
}

static uint64_t server_runtime_current_time_ms(void)
{
    time_t now = time(NULL);

    if (now <= (time_t)0) {
        return 0U;
    }
    return (uint64_t)now * 1000U;
}

static void server_runtime_encode_binary_time_6(uint64_t timestamp_ms, uint8_t* binary_time, size_t binary_time_length)
{
    static const uint64_t unix_to_iec61850_epoch_days = 5113U;
    uint64_t unix_days = timestamp_ms / 86400000U;
    uint64_t milliseconds_of_day = timestamp_ms % 86400000U;
    uint64_t days_since_1984 = unix_days > unix_to_iec61850_epoch_days ? unix_days - unix_to_iec61850_epoch_days : 0U;

    if (binary_time == NULL || binary_time_length < 6U) {
        return;
    }

    binary_time[0] = (uint8_t)((milliseconds_of_day >> 24U) & 0xFFU);
    binary_time[1] = (uint8_t)((milliseconds_of_day >> 16U) & 0xFFU);
    binary_time[2] = (uint8_t)((milliseconds_of_day >> 8U) & 0xFFU);
    binary_time[3] = (uint8_t)(milliseconds_of_day & 0xFFU);
    binary_time[4] = (uint8_t)((days_since_1984 >> 8U) & 0xFFU);
    binary_time[5] = (uint8_t)(days_since_1984 & 0xFFU);
}

static void server_runtime_update_report_sequence(UnitLabMmsServerRuntime* server_runtime)
{
    uint64_t timestamp_ms;

    if (server_runtime == NULL) {
        return;
    }

    timestamp_ms = server_runtime_current_time_ms();
    if (timestamp_ms <= server_runtime->brcb_entry_id_counter) {
        timestamp_ms = server_runtime->brcb_entry_id_counter + 1U;
    }

    server_runtime->brcb_sq_num++;
    server_runtime->brcb_entry_id_counter = timestamp_ms;
    for (size_t index = 0U; index < sizeof(server_runtime->brcb_entry_id); index++) {
        unsigned int shift = (unsigned int)((sizeof(server_runtime->brcb_entry_id) - index - 1U) * 8U);
        server_runtime->brcb_entry_id[index] = (uint8_t)((timestamp_ms >> shift) & 0xFFU);
    }
    server_runtime_encode_binary_time_6(timestamp_ms, server_runtime->brcb_time_of_entry, sizeof(server_runtime->brcb_time_of_entry));
}

static const char* server_runtime_report_data_ref_with_dataset_prefix(
    const char* data_ref,
    const char* dataset_reference,
    char* buffer,
    size_t buffer_length)
{
    const char* dataset_slash;
    const char* data_ref_slash;
    size_t dataset_prefix_length;

    if (data_ref == NULL || data_ref[0] == '\0') {
        return NULL;
    }
    dataset_slash = dataset_reference != NULL ? strchr(dataset_reference, '/') : NULL;
    data_ref_slash = strchr(data_ref, '/');
    if (dataset_slash == NULL || dataset_slash == dataset_reference || data_ref_slash == NULL || buffer == NULL || buffer_length == 0U) {
        return data_ref;
    }

    dataset_prefix_length = (size_t)(dataset_slash - dataset_reference);
    snprintf(buffer, buffer_length, "%.*s%s", (int)dataset_prefix_length, dataset_reference, data_ref_slash);
    return buffer;
}

static int server_runtime_report_replace_trailing_component(const char* reference, const char* replacement, char* buffer, size_t buffer_length)
{
    const char* separator = NULL;
    size_t prefix_length = 0U;
    size_t replacement_length = 0U;

    if (reference == NULL || replacement == NULL || buffer == NULL || buffer_length == 0U) {
        return 0;
    }
    separator = strrchr(reference, '$');
    if (separator == NULL) {
        separator = strrchr(reference, '.');
    }
    if (separator == NULL) {
        return 0;
    }
    prefix_length = (size_t)(separator - reference) + 1U;
    replacement_length = strlen(replacement);
    if (prefix_length + replacement_length + 1U > buffer_length) {
        return 0;
    }
    memcpy(buffer, reference, prefix_length);
    memcpy(&buffer[prefix_length], replacement, replacement_length);
    buffer[prefix_length + replacement_length] = '\0';
    return 1;
}

static const char* server_runtime_report_signal_reference(const UnitLabIedModelSignal* signal)
{
    if (signal == NULL) {
        return NULL;
    }
    return signal->data_set_entry_variable[0] != '\0' ? signal->data_set_entry_variable : signal->object_reference;
}

static int server_runtime_report_signal_references_same_value_leaf(const UnitLabIedModelSignal* candidate, const UnitLabIedModelSignal* value_signal)
{
    const char* candidate_reference = NULL;
    const char* value_reference = NULL;
    char candidate_value_reference[256U];

    if (candidate == NULL || value_signal == NULL) {
        return 0;
    }
    if (strcmp(candidate->data_attribute_path, "q") != 0 && strcmp(candidate->data_attribute_path, "t") != 0) {
        return 0;
    }
    candidate_reference = server_runtime_report_signal_reference(candidate);
    value_reference = server_runtime_report_signal_reference(value_signal);
    if (candidate_reference == NULL || value_reference == NULL) {
        return 0;
    }
    if (server_runtime_report_replace_trailing_component(candidate_reference, "stVal", candidate_value_reference, sizeof(candidate_value_reference))
        && strcmp(candidate_value_reference, value_reference) == 0) {
        return 1;
    }
    if (server_runtime_report_replace_trailing_component(candidate_reference, "f", candidate_value_reference, sizeof(candidate_value_reference))
        && strcmp(candidate_value_reference, value_reference) == 0) {
        return 1;
    }
    return 0;
}

static int server_runtime_report_member_already_included(const size_t* included_member_indices, size_t included_member_count, size_t member_index)
{
    for (size_t index = 0U; index < included_member_count; index++) {
        if (included_member_indices[index] == member_index) {
            return 1;
        }
    }
    return 0;
}

static size_t server_runtime_report_inclusion_bitstring_value_length(size_t member_count)
{
    return 1U + ((member_count + 7U) / 8U);
}

static void server_runtime_report_inclusion_bitstring_init(uint8_t* inclusion_bitstring, size_t bitstring_size, size_t member_count)
{
    size_t bitstring_length = server_runtime_report_inclusion_bitstring_value_length(member_count);
    if (inclusion_bitstring == NULL || bitstring_size == 0U) {
        return;
    }
    if (bitstring_length > bitstring_size) {
        bitstring_length = bitstring_size;
    }
    memset(inclusion_bitstring, 0, bitstring_size);
    inclusion_bitstring[0] = member_count == 0U ? 0U : (uint8_t)((8U - (member_count % 8U)) % 8U);
}

static int server_runtime_report_inclusion_bit_set(uint8_t* inclusion_bitstring, size_t bitstring_size, size_t member_count, size_t member_index)
{
    size_t byte_index = 1U + (member_index / 8U);
    if (inclusion_bitstring == NULL || member_index >= member_count || byte_index >= bitstring_size) {
        return 0;
    }
    inclusion_bitstring[byte_index] = (uint8_t)(inclusion_bitstring[byte_index] | (uint8_t)(0x80U >> (member_index % 8U)));
    return 1;
}

static int server_runtime_report_inclusion_bitstring_set_all(uint8_t* inclusion_bitstring, size_t bitstring_size, size_t member_count)
{
    server_runtime_report_inclusion_bitstring_init(inclusion_bitstring, bitstring_size, member_count);
    for (size_t index = 0U; index < member_count; index++) {
        if (!server_runtime_report_inclusion_bit_set(inclusion_bitstring, bitstring_size, member_count, index)) {
            return 0;
        }
    }
    return 1;
}

int server_runtime_encode_report_control_block_field_value(
    const UnitLabMmsServerRuntime* server_runtime,
    const char* field_name,
    const char* report_id_reference,
    const char* data_set_reference,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsBerElement value_element;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (field_name == NULL || buffer == NULL || encoded_length == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Report control block field encoding requires a field name, buffer, and encoded_length.");
        return 0;
    }

    if (strcmp(field_name, "Owner") != 0) {
        return server_runtime_encode_report_control_block_structure_field_value(server_runtime, field_name, report_id_reference, data_set_reference, buffer, buffer_length, encoded_length, diagnostic);
    }

    unitlab_mms_ber_element_init(&value_element);
    value_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    value_element.tag.constructed = 0;
    value_element.tag.tag_number = 10U;
    value_element.value_bytes = (const uint8_t*)(server_runtime != NULL && server_runtime->brcb_owner[0] != 0 ? server_runtime->brcb_owner : "");
    value_element.value_length = strlen((const char*)value_element.value_bytes);
    if (!unitlab_mms_ber_write(&value_element, buffer, buffer_length, encoded_length, diagnostic)) {
        return 0;
    }
    return 1;
}

static int server_runtime_encode_report_control_block_structure_field_value(
    const UnitLabMmsServerRuntime* server_runtime,
    const char* field_name,
    const char* report_id_reference,
    const char* data_set_reference,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsBerElement value_element;
    uint8_t bool_value[1U] = { 0x00U };
    uint8_t unsigned_value[4U];
    size_t unsigned_value_length = 0U;
    uint8_t opt_flds[3U];
    uint8_t trg_ops[2U];
    const UnitLabIedModelReportControl* report = server_runtime_active_model_report_control(server_runtime);

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (field_name == NULL || buffer == NULL || encoded_length == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Report control block structure field encoding requires a field name, buffer, and encoded_length.");
        return 0;
    }

    unitlab_mms_ber_element_init(&value_element);
    server_runtime_encode_report_optional_fields_bitstring(server_runtime, report, opt_flds);
    server_runtime_encode_report_trigger_options_bitstring(server_runtime, report, trg_ops);

    if (strcmp(field_name, "RptID") == 0) {
        value_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
        value_element.tag.constructed = 0;
        value_element.tag.tag_number = 10U;
        value_element.value_bytes = (const uint8_t*)(report_id_reference != NULL && report_id_reference[0] != '\0' ? report_id_reference : "LD0/LLN0.BR.Events");
        value_element.value_length = strlen((const char*)value_element.value_bytes);
    }
    else if (strcmp(field_name, "RptEna") == 0 || strcmp(field_name, "GI") == 0 || strcmp(field_name, "PurgeBuf") == 0 || strcmp(field_name, "Resv") == 0) {
        if (strcmp(field_name, "RptEna") == 0 && server_runtime != NULL && server_runtime->brcb_rpt_ena != 0U) {
            bool_value[0] = 0x01U;
        }
        value_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
        value_element.tag.constructed = 0;
        value_element.tag.tag_number = 3U;
        value_element.value_bytes = bool_value;
        value_element.value_length = sizeof(bool_value);
    }
    else if (strcmp(field_name, "DatSet") == 0) {
        value_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
        value_element.tag.constructed = 0;
        value_element.tag.tag_number = 10U;
        value_element.value_bytes = (const uint8_t*)(data_set_reference != NULL && data_set_reference[0] != '\0' ? data_set_reference : "LD0/LLN0$dsEvents");
        value_element.value_length = strlen((const char*)value_element.value_bytes);
    }
    else if (strcmp(field_name, "ConfRev") == 0) {
        if (!server_runtime_encode_unsigned_value(server_runtime_report_conf_rev(report), unsigned_value, sizeof(unsigned_value), &unsigned_value_length, diagnostic)) {
            return 0;
        }
        value_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
        value_element.tag.constructed = 0;
        value_element.tag.tag_number = 6U;
        value_element.value_bytes = unsigned_value;
        value_element.value_length = unsigned_value_length;
    }
    else if (strcmp(field_name, "OptFlds") == 0) {
        value_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
        value_element.tag.constructed = 0;
        value_element.tag.tag_number = 4U;
        value_element.value_bytes = opt_flds;
        value_element.value_length = sizeof(opt_flds);
    }
    else if (strcmp(field_name, "BufTm") == 0) {
        if (!server_runtime_encode_unsigned_value(server_runtime_report_buffer_time_ms(report), unsigned_value, sizeof(unsigned_value), &unsigned_value_length, diagnostic)) {
            return 0;
        }
        value_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
        value_element.tag.constructed = 0;
        value_element.tag.tag_number = 6U;
        value_element.value_bytes = unsigned_value;
        value_element.value_length = unsigned_value_length;
    }
    else if (strcmp(field_name, "SqNum") == 0) {
        if (!server_runtime_encode_unsigned_value(server_runtime != NULL ? server_runtime->brcb_sq_num : 0U, unsigned_value, sizeof(unsigned_value), &unsigned_value_length, diagnostic)) {
            return 0;
        }
        value_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
        value_element.tag.constructed = 0;
        value_element.tag.tag_number = 6U;
        value_element.value_bytes = unsigned_value;
        value_element.value_length = unsigned_value_length;
    }
    else if (strcmp(field_name, "TrgOps") == 0) {
        value_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
        value_element.tag.constructed = 0;
        value_element.tag.tag_number = 4U;
        value_element.value_bytes = trg_ops;
        value_element.value_length = sizeof(trg_ops);
    }
    else if (strcmp(field_name, "IntgPd") == 0) {
        if (!server_runtime_encode_unsigned_value(server_runtime_report_integrity_period_ms(report), unsigned_value, sizeof(unsigned_value), &unsigned_value_length, diagnostic)) {
            return 0;
        }
        value_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
        value_element.tag.constructed = 0;
        value_element.tag.tag_number = 6U;
        value_element.value_bytes = unsigned_value;
        value_element.value_length = unsigned_value_length;
    }
    else if (strcmp(field_name, "EntryID") == 0) {
        static const uint8_t empty_entry_id[8U] = { 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U };
        value_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
        value_element.tag.constructed = 0;
        value_element.tag.tag_number = 9U;
        value_element.value_bytes = server_runtime != NULL ? server_runtime->brcb_entry_id : empty_entry_id;
        value_element.value_length = server_runtime != NULL ? sizeof(server_runtime->brcb_entry_id) : sizeof(empty_entry_id);
    }
    else if (strcmp(field_name, "TimeofEntry") == 0 || strcmp(field_name, "TimeOfEntry") == 0) {
        static const uint8_t empty_binary_time[6U] = { 0U, 0U, 0U, 0U, 0U, 0U };
        value_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
        value_element.tag.constructed = 0;
        value_element.tag.tag_number = 12U;
        value_element.value_bytes = server_runtime != NULL ? server_runtime->brcb_time_of_entry : empty_binary_time;
        value_element.value_length = server_runtime != NULL ? sizeof(server_runtime->brcb_time_of_entry) : sizeof(empty_binary_time);
    }
    else if (strcmp(field_name, "ResvTms") == 0) {
        if (!server_runtime_encode_unsigned_value(server_runtime != NULL ? server_runtime->brcb_resv_tms : 0U, unsigned_value, sizeof(unsigned_value), &unsigned_value_length, diagnostic)) {
            return 0;
        }
        value_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
        value_element.tag.constructed = 0;
        value_element.tag.tag_number = 5U;
        value_element.value_bytes = unsigned_value;
        value_element.value_length = unsigned_value_length;
    }
    else {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "Unsupported report control block structure field.");
        return 0;
    }

    if (!unitlab_mms_ber_write(&value_element, buffer, buffer_length, encoded_length, diagnostic)) {
        return 0;
    }
    return 1;
}

int server_runtime_encode_report_control_block_container_value(
    const UnitLabMmsServerRuntime* server_runtime,
    const char* report_id_reference,
    const char* data_set_reference,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    uint8_t rcb_value_bytes[2048U];
    size_t rcb_value_length = 0U;
    UnitLabMmsBerElement structure_element;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (buffer == NULL || encoded_length == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Report control block container encoding requires a buffer and encoded_length.");
        return 0;
    }

    if (!server_runtime_encode_report_control_block_value(server_runtime, report_id_reference, data_set_reference, rcb_value_bytes, sizeof(rcb_value_bytes), &rcb_value_length, diagnostic)) {
        return 0;
    }

    unitlab_mms_ber_element_init(&structure_element);
    structure_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    structure_element.tag.constructed = 1;
    structure_element.tag.tag_number = 2U;
    structure_element.value_bytes = rcb_value_bytes;
    structure_element.value_length = rcb_value_length;
    if (!unitlab_mms_ber_write(&structure_element, buffer, buffer_length, encoded_length, diagnostic)) {
        return 0;
    }
    return 1;
}

int server_runtime_encode_report_control_block_value(
    const UnitLabMmsServerRuntime* server_runtime,
    const char* report_id_reference,
    const char* data_set_reference,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    const UnitLabIedModelReportControl* report = server_runtime_active_model_report_control(server_runtime);
    const char* const* fields = buffered_report_control_block_fields;
    size_t field_count = sizeof(buffered_report_control_block_fields) / sizeof(buffered_report_control_block_fields[0]);
    uint8_t structure_bytes[2048U];
    size_t structure_length = 0U;

    if (report != NULL && !report->is_buffered) {
        fields = unbuffered_report_control_block_fields;
        field_count = sizeof(unbuffered_report_control_block_fields) / sizeof(unbuffered_report_control_block_fields[0]);
    }

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (buffer == NULL || encoded_length == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Report control block value encoding requires a buffer and encoded_length.");
        return 0;
    }

    for (size_t index = 0U; index < field_count; index++) {
        uint8_t field_bytes[128U];
        size_t field_length = 0U;

        if (!server_runtime_encode_report_control_block_structure_field_value(
                server_runtime,
                fields[index],
                report_id_reference,
                data_set_reference,
                field_bytes,
                sizeof(field_bytes),
                &field_length,
                diagnostic)) {
            return 0;
        }
        if (structure_length + field_length > sizeof(structure_bytes)) {
            server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Report control block structure buffer is too small.");
            return 0;
        }
        memcpy(&structure_bytes[structure_length], field_bytes, field_length);
        structure_length += field_length;
    }

    {
        UnitLabMmsBerElement structure_element;

        unitlab_mms_ber_element_init(&structure_element);
        structure_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
        structure_element.tag.constructed = 1;
        structure_element.tag.tag_number = 2U;
        structure_element.value_bytes = structure_bytes;
        structure_element.value_length = structure_length;
        if (!unitlab_mms_ber_write(&structure_element, buffer, buffer_length, encoded_length, diagnostic)) {
            return 0;
        }
    }

    return 1;
}

static int server_runtime_append_ber(
    uint8_t tag_class,
    int constructed,
    uint32_t tag_number,
    const uint8_t* value_bytes,
    size_t value_length,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* offset,
    UnitLabMmsDiagnostic* diagnostic)
{
    size_t encoded_length = 0U;

    if (buffer == NULL || offset == NULL || *offset > buffer_length) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "BER append requires buffer and offset.");
        return 0;
    }
    if (!server_runtime_encode_ber_element(
            tag_class,
            constructed,
            tag_number,
            value_bytes,
            value_length,
            &buffer[*offset],
            buffer_length - *offset,
            &encoded_length,
            diagnostic)) {
        return 0;
    }
    *offset += encoded_length;
    return 1;
}

int unitlab_mms_server_runtime_has_pending_gi_report(const UnitLabMmsServerRuntime* server_runtime)
{
    return server_runtime != NULL && server_runtime->pending_gi_report != 0U;
}

int unitlab_mms_server_runtime_build_pending_gi_report_bytes(UnitLabMmsServerRuntime* server_runtime, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic)
{
    char report_id_reference[160U];
    char dataset_reference[160U];
    uint8_t service_content[62000U];
    uint8_t variable_list_name[32U];
    uint8_t report_values[60000U];
    uint8_t service_bytes[63000U];
    uint8_t scratch[65535U];
    uint8_t unsigned_value[4U];
    size_t unsigned_value_length = 0U;
    size_t variable_list_name_length = 0U;
    size_t report_values_length = 0U;
    size_t service_content_length = 0U;
    size_t service_length = 0U;
    uint8_t opt_flds[3U];
    uint8_t inclusion_bitstring[65U];
    size_t inclusion_bitstring_length = 0U;
    const uint8_t bool_true[1U] = { 0x01U };
    const uint8_t bool_false[1U] = { 0x00U };
    const uint8_t reason_gi[2U] = { 0x02U, 0x04U };
    const uint8_t reason_data_change[2U] = { 0x02U, 0x80U };
    const uint8_t reason_quality_change[2U] = { 0x02U, 0x40U };
    const uint8_t reason_data_update[2U] = { 0x02U, 0x20U };
    const uint8_t reason_integrity[2U] = { 0x02U, 0x10U };
    const uint8_t* reason_code = reason_gi;
    UnitLabMmsPdu report_pdu;
    const UnitLabIedModelReportControl* report = NULL;
    const UnitLabIedModelDataSet* data_set = NULL;
    size_t member_count = 0U;
    size_t included_member_count = 0U;
    size_t included_member_indices[512U];
    uint32_t report_sequence_number = 0U;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (server_runtime == NULL || buffer == NULL || encoded_length == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Server runtime, buffer, and encoded_length are required for GI report.");
        return 0;
    }

    report = server_runtime_active_model_report_control(server_runtime);
    data_set = server_runtime_model_report_data_set(server_runtime, report);
    server_runtime_encode_report_optional_fields_bitstring(server_runtime, report, opt_flds);
    server_runtime_format_report_id_reference(server_runtime, report, report_id_reference, sizeof(report_id_reference));
    server_runtime_format_dataset_reference(server_runtime, report, data_set, dataset_reference, sizeof(dataset_reference));
    if (data_set == NULL || server_runtime->model_plan == NULL || server_runtime->model_plan->signals == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "InformationReport requires a model-backed DataSet.");
        return 0;
    }
    member_count = data_set->member_count;
    if (member_count > sizeof(included_member_indices) / sizeof(included_member_indices[0])) {
        member_count = sizeof(included_member_indices) / sizeof(included_member_indices[0]);
    }
    inclusion_bitstring_length = server_runtime_report_inclusion_bitstring_value_length(member_count);
    if (inclusion_bitstring_length > sizeof(inclusion_bitstring)) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "InformationReport inclusion bitstring buffer is too small.");
        return 0;
    }
    if (server_runtime->pending_report_kind == UNITLAB_MMS_SERVER_PENDING_REPORT_INTEGRITY) {
        included_member_count = member_count;
        for (size_t index = 0U; index < included_member_count; index++) {
            included_member_indices[index] = index;
        }
        if (!server_runtime_report_inclusion_bitstring_set_all(inclusion_bitstring, sizeof(inclusion_bitstring), member_count)) {
            server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "InformationReport inclusion bitstring buffer is too small.");
            return 0;
        }
        reason_code = reason_integrity;
    } else if ((server_runtime->pending_report_kind == UNITLAB_MMS_SERVER_PENDING_REPORT_DATA_CHANGE
            || server_runtime->pending_report_kind == UNITLAB_MMS_SERVER_PENDING_REPORT_QUALITY_CHANGE
            || server_runtime->pending_report_kind == UNITLAB_MMS_SERVER_PENDING_REPORT_DATA_UPDATE)
        && server_runtime->pending_report_member_mask != 0U
        && server_runtime->pending_report_value_length != 0U) {
        included_member_count = 0U;
        server_runtime_report_inclusion_bitstring_init(inclusion_bitstring, sizeof(inclusion_bitstring), member_count);
        for (size_t changed_index = 0U; changed_index < member_count; changed_index++) {
            const UnitLabIedModelSignal* changed_signal = NULL;
            if (changed_index >= 64U || (server_runtime->pending_report_member_mask & ((uint64_t)1U << changed_index)) == 0U) {
                continue;
            }
            if (!server_runtime_report_member_already_included(included_member_indices, included_member_count, changed_index)) {
                included_member_indices[included_member_count++] = changed_index;
                (void)server_runtime_report_inclusion_bit_set(inclusion_bitstring, sizeof(inclusion_bitstring), member_count, changed_index);
            }
            changed_signal = server_runtime_data_set_member_signal(server_runtime, data_set, changed_index);
            if (changed_signal == NULL) {
                server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "InformationReport changed DataSet member is outside the model signal range.");
                return 0;
            }
            for (size_t member_index = 0U; member_index < member_count; member_index++) {
                const UnitLabIedModelSignal* candidate_signal = server_runtime_data_set_member_signal(server_runtime, data_set, member_index);
                if (!server_runtime_report_member_already_included(included_member_indices, included_member_count, member_index)
                    && candidate_signal != NULL
                    && server_runtime_report_signal_references_same_value_leaf(candidate_signal, changed_signal)) {
                    included_member_indices[included_member_count++] = member_index;
                    (void)server_runtime_report_inclusion_bit_set(inclusion_bitstring, sizeof(inclusion_bitstring), member_count, member_index);
                }
            }
        }
        if (server_runtime->pending_report_kind == UNITLAB_MMS_SERVER_PENDING_REPORT_QUALITY_CHANGE) {
            reason_code = reason_quality_change;
        } else if (server_runtime->pending_report_kind == UNITLAB_MMS_SERVER_PENDING_REPORT_DATA_UPDATE) {
            reason_code = reason_data_update;
        } else {
            reason_code = reason_data_change;
        }
    } else {
        included_member_count = member_count;
        for (size_t index = 0U; index < included_member_count; index++) {
            included_member_indices[index] = index;
        }
        if (!server_runtime_report_inclusion_bitstring_set_all(inclusion_bitstring, sizeof(inclusion_bitstring), member_count)) {
            server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "InformationReport inclusion bitstring buffer is too small.");
            return 0;
        }
        server_runtime->pending_report_kind = UNITLAB_MMS_SERVER_PENDING_REPORT_GI;
        reason_code = reason_gi;
    }

    server_runtime_update_report_sequence(server_runtime);
    report_sequence_number = server_runtime->brcb_sq_num == 0U ? 0U : server_runtime->brcb_sq_num - 1U;

    if (!server_runtime_append_ber(UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 0, 0U, (const uint8_t*)"RPT", 3U, variable_list_name, sizeof(variable_list_name), &variable_list_name_length, diagnostic)) {
        return 0;
    }
    if (!server_runtime_append_ber(UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 1, 1U, variable_list_name, variable_list_name_length, service_content, sizeof(service_content), &service_content_length, diagnostic)) {
        return 0;
    }

    if (!server_runtime_append_ber(UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 0, 10U, (const uint8_t*)report_id_reference, strlen(report_id_reference), report_values, sizeof(report_values), &report_values_length, diagnostic)
        || !server_runtime_append_ber(UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 0, 4U, opt_flds, sizeof(opt_flds), report_values, sizeof(report_values), &report_values_length, diagnostic)) {
        return 0;
    }
    if (server_runtime_report_optional_field_enabled(server_runtime, report, UNITLAB_IED_MODEL_RPT_OPT_SEQ_NUM)) {
        if (!server_runtime_encode_unsigned_value(report_sequence_number, unsigned_value, sizeof(unsigned_value), &unsigned_value_length, diagnostic)
            || !server_runtime_append_ber(UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 0, 6U, unsigned_value, unsigned_value_length, report_values, sizeof(report_values), &report_values_length, diagnostic)) {
            return 0;
        }
    }
    if (server_runtime_report_optional_field_enabled(server_runtime, report, UNITLAB_IED_MODEL_RPT_OPT_TIME_STAMP)) {
        if (!server_runtime_append_ber(UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 0, 12U, server_runtime->brcb_time_of_entry, sizeof(server_runtime->brcb_time_of_entry), report_values, sizeof(report_values), &report_values_length, diagnostic)) {
            return 0;
        }
    }
    if (server_runtime_report_optional_field_enabled(server_runtime, report, UNITLAB_IED_MODEL_RPT_OPT_DATA_SET)) {
        if (!server_runtime_append_ber(UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 0, 10U, (const uint8_t*)dataset_reference, strlen(dataset_reference), report_values, sizeof(report_values), &report_values_length, diagnostic)) {
            return 0;
        }
    }
    if (server_runtime_report_optional_field_enabled(server_runtime, report, UNITLAB_IED_MODEL_RPT_OPT_BUFFER_OVERFLOW)) {
        const uint8_t* overflow_value = server_runtime->brcb_buffer_overflow != 0U ? bool_true : bool_false;
        if (!server_runtime_append_ber(UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 0, 3U, overflow_value, 1U, report_values, sizeof(report_values), &report_values_length, diagnostic)) {
            return 0;
        }
    }
    if (server_runtime_report_optional_field_enabled(server_runtime, report, UNITLAB_IED_MODEL_RPT_OPT_ENTRY_ID)) {
        if (!server_runtime_append_ber(UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 0, 9U, server_runtime->brcb_entry_id, sizeof(server_runtime->brcb_entry_id), report_values, sizeof(report_values), &report_values_length, diagnostic)) {
            return 0;
        }
    }
    if (server_runtime_report_optional_field_enabled(server_runtime, report, UNITLAB_IED_MODEL_RPT_OPT_CONF_REV)) {
        if (!server_runtime_encode_unsigned_value(server_runtime_report_conf_rev(report), unsigned_value, sizeof(unsigned_value), &unsigned_value_length, diagnostic)
            || !server_runtime_append_ber(UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 0, 6U, unsigned_value, unsigned_value_length, report_values, sizeof(report_values), &report_values_length, diagnostic)) {
            return 0;
        }
    }
    if (!server_runtime_append_ber(UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 0, 4U, inclusion_bitstring, inclusion_bitstring_length, report_values, sizeof(report_values), &report_values_length, diagnostic)) {
        return 0;
    }

    if (server_runtime_report_optional_field_enabled(server_runtime, report, UNITLAB_IED_MODEL_RPT_OPT_DATA_REFERENCE)) {
        for (size_t included_index = 0U; included_index < included_member_count; included_index++) {
            size_t index = included_member_indices[included_index];
            const char* data_ref = NULL;
            char normalized_data_ref[192U];
            const UnitLabIedModelSignal* signal = server_runtime_data_set_member_signal(server_runtime, data_set, index);
            if (signal == NULL) {
                server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "InformationReport DataSet member is outside the model signal range.");
                return 0;
            }
            data_ref = signal->data_set_entry_variable[0] != '\0' ? signal->data_set_entry_variable : signal->object_reference;
            data_ref = server_runtime_report_data_ref_with_dataset_prefix(data_ref, dataset_reference, normalized_data_ref, sizeof(normalized_data_ref));
            if (data_ref == NULL || data_ref[0] == '\0') {
                server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "GI report DataSet member is missing a data reference.");
                return 0;
            }
            if (!server_runtime_append_ber(UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 0, 10U, (const uint8_t*)data_ref, strlen(data_ref), report_values, sizeof(report_values), &report_values_length, diagnostic)) {
                return 0;
            }
        }
    }

    for (size_t included_index = 0U; included_index < included_member_count; included_index++) {
        size_t index = included_member_indices[included_index];
        uint8_t value_bytes[512U];
        size_t value_length = 0U;
        const UnitLabIedModelSignal* signal = NULL;
        signal = server_runtime_data_set_member_signal(server_runtime, data_set, index);
        if (signal == NULL) {
            server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "InformationReport DataSet member is outside the model signal range.");
            return 0;
        }
        if (index < UNITLAB_MMS_SERVER_RUNTIME_MAX_REPORT_MEMBERS
            && server_runtime->pending_report_value_lengths[index] != 0U
            && (server_runtime->pending_report_kind == UNITLAB_MMS_SERVER_PENDING_REPORT_DATA_CHANGE
                || server_runtime->pending_report_kind == UNITLAB_MMS_SERVER_PENDING_REPORT_DATA_UPDATE
                || server_runtime->pending_report_kind == UNITLAB_MMS_SERVER_PENDING_REPORT_QUALITY_CHANGE)) {
            if (server_runtime->pending_report_value_lengths[index] > sizeof(value_bytes)) {
                server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Pending report snapshot value is too large.");
                return 0;
            }
            memcpy(value_bytes, server_runtime->pending_report_values[index], server_runtime->pending_report_value_lengths[index]);
            value_length = server_runtime->pending_report_value_lengths[index];
        } else if (strcmp(signal->data_attribute_path, "q") == 0) {
            if (!server_runtime_encode_current_signal_quality(server_runtime, signal, value_bytes, sizeof(value_bytes), &value_length, diagnostic)) {
                return 0;
            }
        } else if (strcmp(signal->data_attribute_path, "t") == 0) {
            if (!server_runtime_encode_current_signal_timestamp(server_runtime, signal, value_bytes, sizeof(value_bytes), &value_length, diagnostic)) {
                return 0;
            }
        } else if (!server_runtime_encode_current_signal_value(server_runtime, signal, value_bytes, sizeof(value_bytes), &value_length, diagnostic)) {
            return 0;
        }
        if (report_values_length + value_length > sizeof(report_values)) {
            server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "InformationReport value buffer is too small.");
            return 0;
        }
        memcpy(&report_values[report_values_length], value_bytes, value_length);
        report_values_length += value_length;
    }

    if (server_runtime_report_optional_field_enabled(server_runtime, report, UNITLAB_IED_MODEL_RPT_OPT_REASON_FOR_INCLUSION)) {
        for (size_t index = 0U; index < included_member_count; index++) {
            if (!server_runtime_append_ber(UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 0, 4U, reason_code, 2U, report_values, sizeof(report_values), &report_values_length, diagnostic)) {
                return 0;
            }
        }
    }

    if (!server_runtime_append_ber(UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 1, 0U, report_values, report_values_length, service_content, sizeof(service_content), &service_content_length, diagnostic)) {
        return 0;
    }
    if (!server_runtime_append_ber(UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 1, 0U, service_content, service_content_length, service_bytes, sizeof(service_bytes), &service_length, diagnostic)) {
        return 0;
    }

    unitlab_mms_pdu_init(&report_pdu);
    report_pdu.kind = UNITLAB_MMS_PDU_UNCONFIRMED;
    report_pdu.has_service = 1;
    report_pdu.service_kind = UNITLAB_MMS_SERVICE_INFORMATION_REPORT;
    report_pdu.pdu_bytes = service_bytes;
    report_pdu.pdu_length = service_length;
    if (!unitlab_mms_build_wire_frame_from_pdu(&report_pdu, scratch, sizeof(scratch), buffer, buffer_length, encoded_length, diagnostic)) {
        return 0;
    }
    server_runtime->brcb_buffer_overflow = 0U;
    server_runtime->reports_sent++;
    printf(
        "native-wire-server: report-sent rptId=%s datSet=%s sqNum=%u kind=%u included-members=%zu queue-depth=%zu queued=%llu coalesced=%llu dropped=%llu sent=%llu\n",
        report_id_reference[0] != '\0' ? report_id_reference : "<none>",
        dataset_reference[0] != '\0' ? dataset_reference : "<none>",
        (unsigned)report_sequence_number,
        (unsigned)server_runtime->pending_report_kind,
        included_member_count,
        server_runtime->pending_report_queue_count,
        (unsigned long long)server_runtime->report_events_queued,
        (unsigned long long)server_runtime->report_events_coalesced,
        (unsigned long long)server_runtime->report_events_dropped,
        (unsigned long long)server_runtime->reports_sent);
    fflush(stdout);
    server_runtime_advance_pending_report_queue(server_runtime);
    if (server_runtime->report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_GI_PENDING) {
        UnitLabMmsDiagnostic report_diagnostic;
        unitlab_mms_diagnostic_clear(&report_diagnostic);
        (void)unitlab_iec61850_report_control_accept_report(&server_runtime->report_control, server_runtime->brcb_sq_num, &report_diagnostic);
    }
    server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}

