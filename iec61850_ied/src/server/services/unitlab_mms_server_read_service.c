#include "server/unitlab_mms_server_runtime_internal.h"

#include <stdio.h>
#include <string.h>

/* Read service handles ReadResponse construction, AccessResult encoding, and current value lookup. */

static const char* server_runtime_ber_tag_class_label(UnitLabMmsBerTagClass tag_class)
{
    switch (tag_class) {
        case UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL:
            return "UNIVERSAL";
        case UNITLAB_MMS_BER_TAG_CLASS_APPLICATION:
            return "APPLICATION";
        case UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC:
            return "CONTEXT-SPECIFIC";
        case UNITLAB_MMS_BER_TAG_CLASS_PRIVATE:
            return "PRIVATE";
        default:
            return "UNKNOWN";
    }
}

static void server_runtime_log_ber_element_line(const char* prefix, const UnitLabMmsBerElement* element)
{
    if (prefix == NULL || element == NULL) {
        return;
    }
    printf(
        "%s tag=%s constructed=%u number=%u length=%zu\n",
        prefix,
        server_runtime_ber_tag_class_label(element->tag.tag_class),
        (unsigned)element->tag.constructed,
        (unsigned)element->tag.tag_number,
        element->value_length);
}

static void server_runtime_log_read_response_tree(
    uint32_t invoke_id,
    const uint8_t* service_bytes,
    size_t service_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsBerElement invoke_id_element;
    UnitLabMmsBerElement service_element;
    UnitLabMmsBerElement list_of_access_result_element;
    UnitLabMmsBerElement access_result_element;
    UnitLabMmsBerElement data_element;
    char data_tag_hex[32U];
    size_t consumed_length = 0U;
    size_t response_consumed_length = 0U;
    size_t list_of_access_result_consumed_length = 0U;
    size_t access_result_consumed_length = 0U;
    size_t data_consumed_length = 0U;
    const uint8_t* access_result_list_bytes = NULL;
    size_t access_result_list_length = 0U;
    size_t offset = 0U;
    size_t access_result_count = 0U;
    int access_result_success = 0;

    if (service_bytes == NULL || service_length == 0U) {
        return;
    }

    unitlab_mms_ber_element_init(&invoke_id_element);
    if (!unitlab_mms_ber_read(&invoke_id_element, service_bytes, service_length, &consumed_length, diagnostic)) {
        printf("native-wire-server: read-ber invoke=%u decode-failed-at-invoke\n", (unsigned)invoke_id);
        fflush(stdout);
        return;
    }
    server_runtime_log_ber_element_line("native-wire-server: read-ber invoke-id", &invoke_id_element);

    unitlab_mms_ber_element_init(&service_element);
    if (!unitlab_mms_ber_read(&service_element, &service_bytes[consumed_length], service_length - consumed_length, &response_consumed_length, diagnostic)) {
        printf("native-wire-server: read-ber invoke=%u decode-failed-at-response\n", (unsigned)invoke_id);
        fflush(stdout);
        return;
    }
    server_runtime_log_ber_element_line("native-wire-server: read-ber response", &service_element);

    access_result_list_bytes = service_element.value_bytes;
    access_result_list_length = service_element.value_length;
    unitlab_mms_ber_element_init(&list_of_access_result_element);
    if (unitlab_mms_ber_read(&list_of_access_result_element, service_element.value_bytes, service_element.value_length, &list_of_access_result_consumed_length, diagnostic)
        && list_of_access_result_consumed_length == service_element.value_length
        && list_of_access_result_element.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC
        && list_of_access_result_element.tag.tag_number == 1U) {
        server_runtime_log_ber_element_line("native-wire-server: read-ber listOfAccessResult", &list_of_access_result_element);
        access_result_list_bytes = list_of_access_result_element.value_bytes;
        access_result_list_length = list_of_access_result_element.value_length;
    }

    while (offset < access_result_list_length) {
        unitlab_mms_ber_element_init(&access_result_element);
        if (!unitlab_mms_ber_read(&access_result_element, &access_result_list_bytes[offset], access_result_list_length - offset, &access_result_consumed_length, diagnostic)) {
            printf("native-wire-server: read-ber invoke=%u accessResult[%zu] decode-failed\n", (unsigned)invoke_id, access_result_count);
            fflush(stdout);
            return;
        }
        printf(
            "native-wire-server: read-ber invoke=%u accessResult[%zu] choice tag=%s constructed=%u number=%u length=%zu\n",
            (unsigned)invoke_id,
            access_result_count,
            server_runtime_ber_tag_class_label(access_result_element.tag.tag_class),
            (unsigned)access_result_element.tag.constructed,
            (unsigned)access_result_element.tag.tag_number,
            access_result_element.value_length);

        unitlab_mms_ber_element_init(&data_element);
        data_tag_hex[0] = '\0';
        access_result_success = !(access_result_element.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC
            && access_result_element.tag.tag_number == 0U);
        if (access_result_element.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC) {
            data_element = access_result_element;
            data_consumed_length = access_result_consumed_length;
        }
        else if (unitlab_mms_ber_read(&data_element, access_result_element.value_bytes, access_result_element.value_length, &data_consumed_length, diagnostic)) {
        }
        else {
            data_consumed_length = 0U;
        }
        if (data_consumed_length != 0U) {
            (void)server_runtime_tag_to_hex(&data_element.tag, data_tag_hex, sizeof(data_tag_hex));
            if (server_runtime_bytes_are_printable_ascii(data_element.value_bytes, data_element.value_length)) {
                char printable_value[128U];
                size_t printable_length = data_element.value_length < sizeof(printable_value) - 1U ? data_element.value_length : sizeof(printable_value) - 1U;
                memcpy(printable_value, data_element.value_bytes, printable_length);
                printable_value[printable_length] = '\0';
                printf(
                    "native-wire-server: read-ber invoke=%u accessResult[%zu] status=%s data-tag=%s value-length=%zu value-string=\"%s\"\n",
                    (unsigned)invoke_id,
                    access_result_count,
                    access_result_success ? "success" : "failure",
                    data_tag_hex[0] != '\0' ? data_tag_hex : "<invalid>",
                    data_element.value_length,
                    printable_value);
            } else {
                printf(
                    "native-wire-server: read-ber invoke=%u accessResult[%zu] status=%s data-tag=%s value-length=%zu\n",
                    (unsigned)invoke_id,
                    access_result_count,
                    access_result_success ? "success" : "failure",
                    data_tag_hex[0] != '\0' ? data_tag_hex : "<invalid>",
                    data_element.value_length);
            }
        } else {
            printf(
                "native-wire-server: read-ber invoke=%u accessResult[%zu] status=%s data-decode-failed\n",
                (unsigned)invoke_id,
                access_result_count,
                access_result_success ? "success" : "failure");
        }
        fflush(stdout);
        offset += access_result_consumed_length;
        access_result_count++;
    }
    printf("native-wire-server: read-ber invoke=%u accessResult-count=%zu\n", (unsigned)invoke_id, access_result_count);
    fflush(stdout);
}

static int server_runtime_encode_nested_integer_structure_value(
    int32_t value,
    size_t nested_structure_count,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    uint8_t integer_storage[5U];
    uint8_t current_storage[256U];
    uint8_t next_storage[256U];
    UnitLabMmsBerElement element;
    size_t integer_length = 0U;
    size_t current_length = 0U;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (buffer == NULL || encoded_length == NULL || nested_structure_count == 0U) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Nested integer structure encoding requires output pointers and at least one structure.");
        return 0;
    }
    if (!server_runtime_encode_signed_integer(value, integer_storage, sizeof(integer_storage), &integer_length, diagnostic)) {
        return 0;
    }

    unitlab_mms_ber_element_init(&element);
    element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    element.tag.constructed = 0;
    element.tag.tag_number = 5U;
    element.value_bytes = integer_storage;
    element.value_length = integer_length;
    if (!unitlab_mms_ber_write(&element, current_storage, sizeof(current_storage), &current_length, diagnostic)) {
        return 0;
    }

    for (size_t index = 0U; index < nested_structure_count; index++) {
        size_t next_length = 0U;

        unitlab_mms_ber_element_init(&element);
        element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
        element.tag.constructed = 1;
        element.tag.tag_number = 2U;
        element.value_bytes = current_storage;
        element.value_length = current_length;
        if (!unitlab_mms_ber_write(&element, next_storage, sizeof(next_storage), &next_length, diagnostic)) {
            return 0;
        }
        if (next_length > sizeof(current_storage)) {
            server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Nested integer structure buffer is too small.");
            return 0;
        }
        memcpy(current_storage, next_storage, next_length);
        current_length = next_length;
    }

    if (current_length > buffer_length) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Nested integer structure output buffer is too small.");
        return 0;
    }
    memcpy(buffer, current_storage, current_length);
    *encoded_length = current_length;
    return 1;
}


static int server_runtime_encode_nested_data_structure_value(
    const uint8_t* value_bytes,
    size_t value_length,
    size_t nested_structure_count,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    uint8_t current_storage[256U];
    uint8_t next_storage[256U];
    size_t current_length = 0U;
    UnitLabMmsBerElement element;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (value_bytes == NULL || value_length == 0U || buffer == NULL || encoded_length == NULL || nested_structure_count == 0U) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Nested structure encoding requires a value and output buffer.");
        return 0;
    }
    if (value_length > sizeof(current_storage)) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Nested structure source value is too large.");
        return 0;
    }
    memcpy(current_storage, value_bytes, value_length);
    current_length = value_length;

    for (size_t index = 0U; index < nested_structure_count; index++) {
        size_t next_length = 0U;

        unitlab_mms_ber_element_init(&element);
        element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
        element.tag.constructed = 1;
        element.tag.tag_number = 2U;
        element.value_bytes = current_storage;
        element.value_length = current_length;
        if (!unitlab_mms_ber_write(&element, next_storage, sizeof(next_storage), &next_length, diagnostic)) {
            return 0;
        }
        if (next_length > sizeof(current_storage)) {
            server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Nested structure buffer is too small.");
            return 0;
        }
        memcpy(current_storage, next_storage, next_length);
        current_length = next_length;
    }

    if (current_length > buffer_length) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Nested structure output buffer is too small.");
        return 0;
    }
    memcpy(buffer, current_storage, current_length);
    *encoded_length = current_length;
    return 1;
}

static int server_runtime_encode_nested_signal_structure_value(
    UnitLabMmsServerRuntime* server_runtime,
    const char* object_reference,
    size_t nested_structure_count,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    uint8_t value_bytes[128U];
    size_t value_length = 0U;
    const UnitLabIedModelSignal* signal = server_runtime_find_signal_by_object_reference(server_runtime, object_reference);

    if (signal == NULL) {
        return 0;
    }
    if (!server_runtime_encode_current_signal_value(server_runtime, signal, value_bytes, sizeof(value_bytes), &value_length, diagnostic)) {
        return 0;
    }
    return server_runtime_encode_nested_data_structure_value(value_bytes, value_length, nested_structure_count, buffer, buffer_length, encoded_length, diagnostic);
}

static int server_runtime_build_read_response_value(
    UnitLabMmsServerRuntime* server_runtime,
    const char* object_reference,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    int* value_supported,
    UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsBerElement value_element;
    UnitLabIedModelSignal synthetic_signal;
    const UnitLabIedModelSignal* signal = NULL;
    int32_t integer_value = 0;
    uint8_t integer_bytes[5U];
    uint8_t value_single[1U];
    char rcb_report_id_reference[256U];
    char rcb_data_set_reference[256U];
    size_t integer_length = 0U;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (value_supported != NULL) {
        *value_supported = 0;
    }
    if (server_runtime == NULL || object_reference == NULL || buffer == NULL || encoded_length == NULL || value_supported == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Read response value encoding requires a server runtime, object reference, buffer, and encoded_length.");
        return 0;
    }
    server_runtime_format_report_control_references(
        server_runtime,
        rcb_report_id_reference,
        sizeof(rcb_report_id_reference),
        rcb_data_set_reference,
        sizeof(rcb_data_set_reference));

    if (server_runtime_object_reference_has_suffix(object_reference, ".PGGIO1.ST")) {
        if (!server_runtime_encode_nested_signal_structure_value(server_runtime, "PGGIO1.ST.Ind1.stVal", 2U, buffer, buffer_length, encoded_length, diagnostic)
            && !server_runtime_encode_nested_integer_structure_value(1, 2U, buffer, buffer_length, encoded_length, diagnostic)) {
            return 0;
        }
        *value_supported = 1;
        return 1;
    }
    if (server_runtime_object_reference_has_suffix(object_reference, ".GGIO1.MX")) {
        if (!server_runtime_encode_nested_signal_structure_value(server_runtime, "GGIO1.MX.AnIn1.mag.f", 3U, buffer, buffer_length, encoded_length, diagnostic)
            && !server_runtime_encode_nested_integer_structure_value(0, 3U, buffer, buffer_length, encoded_length, diagnostic)) {
            return 0;
        }
        *value_supported = 1;
        return 1;
    }
    if (server_runtime_object_reference_has_suffix(object_reference, ".XCBR1.ST")) {
        if (!server_runtime_encode_nested_signal_structure_value(server_runtime, "XCBR1.ST.Pos.stVal", 2U, buffer, buffer_length, encoded_length, diagnostic)
            && !server_runtime_encode_nested_integer_structure_value(0, 2U, buffer, buffer_length, encoded_length, diagnostic)) {
            return 0;
        }
        *value_supported = 1;
        return 1;
    }

    signal = server_runtime_find_signal_by_object_reference(server_runtime, object_reference);
    if (signal != NULL) {
        if (server_runtime_encode_current_signal_value(server_runtime, signal, buffer, buffer_length, encoded_length, diagnostic)) {
            *value_supported = 1;
            return 1;
        }
        if (diagnostic != NULL && diagnostic->code != UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED) {
            return 0;
        }
    }


    unitlab_mms_ber_element_init(&value_element);
    memset(&synthetic_signal, 0, sizeof(synthetic_signal));

    {
        const UnitLabIedModelNamespaceAttribute* namespace_attribute = unitlab_find_ied_model_namespace_attribute(server_runtime->model_plan, object_reference);
        if (namespace_attribute != NULL) {
            synthetic_signal.initial_value_kind = namespace_attribute->initial_value_kind;
            snprintf(synthetic_signal.initial_value, sizeof(synthetic_signal.initial_value), "%s", namespace_attribute->initial_value);
            if (!server_runtime_encode_mms_data_value(&synthetic_signal, buffer, buffer_length, encoded_length, diagnostic)) {
                return 0;
            }
            *value_supported = 1;
            return 1;
        }
    }

    if (server_runtime_object_reference_has_suffix(object_reference, ".EX.NamPlt.ldNs")) {
        synthetic_signal.initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_STRING;
        snprintf(synthetic_signal.initial_value, sizeof(synthetic_signal.initial_value), "%s", "LD0");
        if (!server_runtime_encode_mms_data_value(&synthetic_signal, buffer, buffer_length, encoded_length, diagnostic)) {
            return 0;
        }
        *value_supported = 1;
        return 1;
    }
    if (server_runtime_object_reference_has_suffix(object_reference, ".EX.NamPlt.lnNs")) {
        synthetic_signal.initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_STRING;
        snprintf(synthetic_signal.initial_value, sizeof(synthetic_signal.initial_value), "%s", "IEC 61850-7-4:2007");
        if (!server_runtime_encode_mms_data_value(&synthetic_signal, buffer, buffer_length, encoded_length, diagnostic)) {
            return 0;
        }
        *value_supported = 1;
        return 1;
    }
    if (server_runtime_object_reference_has_suffix(object_reference, ".cdcNs")) {
        synthetic_signal.initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_STRING;
        snprintf(synthetic_signal.initial_value, sizeof(synthetic_signal.initial_value), "%s", "IEC 61850-7-3:2010");
        if (!server_runtime_encode_mms_data_value(&synthetic_signal, buffer, buffer_length, encoded_length, diagnostic)) {
            return 0;
        }
        *value_supported = 1;
        return 1;
    }
    if (server_runtime_object_reference_has_suffix(object_reference, ".dataNs")) {
        synthetic_signal.initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_STRING;
        snprintf(synthetic_signal.initial_value, sizeof(synthetic_signal.initial_value), "%s", "EXT:2015");
        if (!server_runtime_encode_mms_data_value(&synthetic_signal, buffer, buffer_length, encoded_length, diagnostic)) {
            return 0;
        }
        *value_supported = 1;
        return 1;
    }
    if (server_runtime_object_reference_has_suffix(object_reference, ".DC.NamPlt.vendor")) {
        synthetic_signal.initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_STRING;
        snprintf(synthetic_signal.initial_value, sizeof(synthetic_signal.initial_value), "%s", "UnitLab");
        if (!server_runtime_encode_mms_data_value(&synthetic_signal, buffer, buffer_length, encoded_length, diagnostic)) {
            return 0;
        }
        *value_supported = 1;
        return 1;
    }
    if (server_runtime_object_reference_has_suffix(object_reference, ".DC.NamPlt.swRev")) {
        synthetic_signal.initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_STRING;
        snprintf(synthetic_signal.initial_value, sizeof(synthetic_signal.initial_value), "%s", "1.0");
        if (!server_runtime_encode_mms_data_value(&synthetic_signal, buffer, buffer_length, encoded_length, diagnostic)) {
            return 0;
        }
        *value_supported = 1;
        return 1;
    }
    if (server_runtime_object_reference_has_suffix(object_reference, ".DC.NamPlt.d")) {
        synthetic_signal.initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_STRING;
        snprintf(synthetic_signal.initial_value, sizeof(synthetic_signal.initial_value), "%s", "LD0");
        if (!server_runtime_encode_mms_data_value(&synthetic_signal, buffer, buffer_length, encoded_length, diagnostic)) {
            return 0;
        }
        *value_supported = 1;
        return 1;
    }
    if (server_runtime_object_reference_has_suffix(object_reference, ".DC.NamPlt.configRev")) {
        synthetic_signal.initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_STRING;
        snprintf(synthetic_signal.initial_value, sizeof(synthetic_signal.initial_value), "%s", "1");
        if (!server_runtime_encode_mms_data_value(&synthetic_signal, buffer, buffer_length, encoded_length, diagnostic)) {
            return 0;
        }
        *value_supported = 1;
        return 1;
    }
    {
        size_t rcb_field_count = 0U;
        const char* const* rcb_fields = server_runtime_report_control_block_fields(&rcb_field_count);
        for (size_t rcb_field_index = 0U; rcb_field_index < rcb_field_count; rcb_field_index++) {
            char brcb_suffix[128U];
            char legacy_suffix[160U];

            snprintf(brcb_suffix, sizeof(brcb_suffix), ".BR.brcbEvents.%s", rcb_fields[rcb_field_index]);
            snprintf(legacy_suffix, sizeof(legacy_suffix), ".BR.LLN0_Events_BuffRep01.%s", rcb_fields[rcb_field_index]);
            (void)brcb_suffix;
            (void)legacy_suffix;
            if (server_runtime_object_reference_matches_report_control_field(object_reference, rcb_fields[rcb_field_index])) {
                if (!server_runtime_encode_report_control_block_field_value(
                        server_runtime,
                        rcb_fields[rcb_field_index],
                        rcb_report_id_reference,
                        rcb_data_set_reference,
                        buffer,
                        buffer_length,
                        encoded_length,
                        diagnostic)) {
                    return 0;
                }
                *value_supported = 1;
                return 1;
            }
        }
    }
    if (server_runtime_object_reference_has_suffix_any(object_reference, (const char* const[]){ ".BR.brcbEvents.RptID", ".BR.LLN0_Events_BuffRep01.RptID" }, 2U)) {
        synthetic_signal.initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_STRING;
        snprintf(synthetic_signal.initial_value, sizeof(synthetic_signal.initial_value), "%s", "IED1LD0/LLN0.BR.Events");
        if (!server_runtime_encode_mms_data_value(&synthetic_signal, buffer, buffer_length, encoded_length, diagnostic)) {
            return 0;
        }
        *value_supported = 1;
        return 1;
    }
    if (server_runtime_object_reference_has_suffix_any(object_reference, (const char* const[]){ ".BR.brcbEvents.DatSet", ".BR.LLN0_Events_BuffRep01.DatSet" }, 2U)) {
        if (!server_runtime_encode_report_control_block_field_value(server_runtime, "DatSet", rcb_report_id_reference, rcb_data_set_reference, buffer, buffer_length, encoded_length, diagnostic)) {
            return 0;
        }
        *value_supported = 1;
        return 1;
    }
    if (server_runtime_object_reference_has_suffix_any(object_reference, (const char* const[]){ ".BR.brcbEvents.ConfRev", ".BR.LLN0_Events_BuffRep01.ConfRev" }, 2U)) {
        integer_value = 7;
        if (!server_runtime_encode_signed_integer(integer_value, integer_bytes, sizeof(integer_bytes), &integer_length, diagnostic)) {
            return 0;
        }
        value_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL;
        value_element.tag.constructed = 0;
        value_element.tag.tag_number = 2U;
        value_element.value_bytes = &integer_bytes[sizeof(integer_bytes) - integer_length];
        value_element.value_length = integer_length;
        if (!unitlab_mms_ber_write(&value_element, buffer, buffer_length, encoded_length, diagnostic)) {
            return 0;
        }
        *value_supported = 1;
        return 1;
    }
    if (server_runtime_object_reference_has_suffix_any(object_reference, (const char* const[]){ ".BR.brcbEvents.BufTm", ".BR.LLN0_Events_BuffRep01.BufTm" }, 2U)) {
        integer_value = 100;
        if (!server_runtime_encode_signed_integer(integer_value, integer_bytes, sizeof(integer_bytes), &integer_length, diagnostic)) {
            return 0;
        }
        value_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL;
        value_element.tag.constructed = 0;
        value_element.tag.tag_number = 2U;
        value_element.value_bytes = &integer_bytes[sizeof(integer_bytes) - integer_length];
        value_element.value_length = integer_length;
        if (!unitlab_mms_ber_write(&value_element, buffer, buffer_length, encoded_length, diagnostic)) {
            return 0;
        }
        *value_supported = 1;
        return 1;
    }
    if (server_runtime_object_reference_has_suffix_any(object_reference, (const char* const[]){ ".BR.brcbEvents.IntgPd", ".BR.LLN0_Events_BuffRep01.IntgPd" }, 2U)) {
        integer_value = 1000;
        if (!server_runtime_encode_signed_integer(integer_value, integer_bytes, sizeof(integer_bytes), &integer_length, diagnostic)) {
            return 0;
        }
        value_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL;
        value_element.tag.constructed = 0;
        value_element.tag.tag_number = 2U;
        value_element.value_bytes = &integer_bytes[sizeof(integer_bytes) - integer_length];
        value_element.value_length = integer_length;
        if (!unitlab_mms_ber_write(&value_element, buffer, buffer_length, encoded_length, diagnostic)) {
            return 0;
        }
        *value_supported = 1;
        return 1;
    }
    if (server_runtime_object_reference_has_suffix_any(object_reference, (const char* const[]){ ".BR.brcbEvents.SqNum", ".BR.LLN0_Events_BuffRep01.SqNum" }, 2U)
        || server_runtime_object_reference_has_suffix_any(object_reference, (const char* const[]){ ".BR.brcbEvents.ResvTms", ".BR.LLN0_Events_BuffRep01.ResvTms" }, 2U)) {
        integer_value = server_runtime_object_reference_has_suffix_any(object_reference, (const char* const[]){ ".BR.brcbEvents.ResvTms", ".BR.LLN0_Events_BuffRep01.ResvTms" }, 2U) ? (int32_t)server_runtime->brcb_resv_tms : 0;
        if (!server_runtime_encode_signed_integer(integer_value, integer_bytes, sizeof(integer_bytes), &integer_length, diagnostic)) {
            return 0;
        }
        value_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL;
        value_element.tag.constructed = 0;
        value_element.tag.tag_number = 2U;
        value_element.value_bytes = &integer_bytes[sizeof(integer_bytes) - integer_length];
        value_element.value_length = integer_length;
        if (!unitlab_mms_ber_write(&value_element, buffer, buffer_length, encoded_length, diagnostic)) {
            return 0;
        }
        *value_supported = 1;
        return 1;
    }
    if (server_runtime_object_reference_has_suffix_any(object_reference, (const char* const[]){ ".BR.brcbEvents.GI", ".BR.LLN0_Events_BuffRep01.GI" }, 2U)
        || server_runtime_object_reference_has_suffix_any(object_reference, (const char* const[]){ ".BR.brcbEvents.PurgeBuf", ".BR.LLN0_Events_BuffRep01.PurgeBuf" }, 2U)
        || server_runtime_object_reference_has_suffix_any(object_reference, (const char* const[]){ ".BR.brcbEvents.RptEna", ".BR.LLN0_Events_BuffRep01.RptEna" }, 2U)) {
        uint8_t boolean_value = server_runtime_object_reference_has_suffix_any(object_reference, (const char* const[]){ ".BR.brcbEvents.RptEna", ".BR.LLN0_Events_BuffRep01.RptEna" }, 2U) && server_runtime->brcb_rpt_ena != 0U ? 0x01U : 0x00U;

        value_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL;
        value_element.tag.constructed = 0;
        value_element.tag.tag_number = 1U;
        value_element.value_bytes = &boolean_value;
        value_element.value_length = 1U;
        if (!unitlab_mms_ber_write(&value_element, buffer, buffer_length, encoded_length, diagnostic)) {
            return 0;
        }
        *value_supported = 1;
        return 1;
    }
    if (server_runtime_object_reference_matches_report_control_object(object_reference)) {
        if (!server_runtime_encode_report_control_block_value(server_runtime, rcb_report_id_reference, rcb_data_set_reference, buffer, buffer_length, encoded_length, diagnostic)) {
            return 0;
        }
        *value_supported = 1;
        return 1;
    }
    if (server_runtime_object_reference_has_suffix(object_reference, ".BR")) {
        if (!server_runtime_encode_report_control_block_container_value(server_runtime, rcb_report_id_reference, rcb_data_set_reference, buffer, buffer_length, encoded_length, diagnostic)) {
            return 0;
        }
        *value_supported = 1;
        return 1;
    }

    if (buffer_length < 1U) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "DataAccessError encoding buffer is too small.");
        return 0;
    }
    value_single[0] = 0x09U;
    buffer[0] = value_single[0];
    *encoded_length = 1U;
    *value_supported = 0;
    return 1;
}

int server_runtime_build_read_response_service(
    UnitLabMmsServerRuntime* server_runtime,
    uint32_t invoke_id,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    uint8_t value_bytes[128U];
    uint8_t access_result_value_bytes[160U];
    uint8_t list_of_access_result_bytes[256U];
    uint8_t list_of_access_result_wrapper_bytes[288U];
    uint8_t read_response_body_bytes[320U];
    uint8_t service_bytes[360U];
    uint8_t invoke_id_element_bytes[16U];
    char domain_id[128U];
    char item_id[128U];
    size_t value_length = 0U;
    size_t access_result_value_length = 0U;
    size_t list_of_access_result_length = 0U;
    size_t list_of_access_result_wrapper_length = 0U;
    size_t read_response_body_length = 0U;
    size_t invoke_id_length = 0U;
    size_t total_length = 0U;
    const char* read_target = NULL;
    int value_supported = 0;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (buffer == NULL || encoded_length == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Read response service buffer and encoded_length are required.");
        return 0;
    }

    domain_id[0] = '\0';
    item_id[0] = '\0';
    if (!server_runtime_parse_object_reference(server_runtime->pending_request.object_reference, domain_id, sizeof(domain_id), item_id, sizeof(item_id))) {
        domain_id[0] = '\0';
        item_id[0] = '\0';
    }
    read_target = server_runtime->pending_request.attribute_reference[0] != '\0'
        ? server_runtime->pending_request.attribute_reference
        : server_runtime->pending_request.object_reference;
    printf(
        "native-wire-server: confirmed-response invoke=%u service=Read object=%s domain=%s item=%s target=%s\n",
        (unsigned)invoke_id,
        server_runtime->pending_request.object_reference[0] != '\0' ? server_runtime->pending_request.object_reference : "<none>",
        domain_id[0] != '\0' ? domain_id : "<none>",
        item_id[0] != '\0' ? item_id : "<none>",
        read_target != NULL && read_target[0] != '\0' ? read_target : "<none>");
    if (strstr(server_runtime->pending_request.object_reference, "NamPlt") != NULL) {
        printf(
            "native-wire-server: read-namespace-request invoke=%u object=%s attribute=%s\n",
            (unsigned)invoke_id,
            server_runtime->pending_request.object_reference[0] != '\0' ? server_runtime->pending_request.object_reference : "<none>",
            server_runtime->pending_request.attribute_reference[0] != '\0' ? server_runtime->pending_request.attribute_reference : "<none>");
    }
    fflush(stdout);

    size_t read_object_reference_count = server_runtime->pending_request.read_object_reference_count;

    printf(
        "native-wire-server: confirmed-response invoke=%u service=Read read_object_reference_count=%zu\n",
        (unsigned)invoke_id,
        read_object_reference_count);
    fflush(stdout);
    if (read_object_reference_count == 0U) {
        read_object_reference_count = 1U;
    }
    for (size_t index = 0U; index < read_object_reference_count; index++) {
        const char* current_object_reference = index < server_runtime->pending_request.read_object_reference_count && server_runtime->pending_request.read_object_references[index][0] != '\0'
            ? server_runtime->pending_request.read_object_references[index]
            : server_runtime->pending_request.object_reference;
        const char* current_attribute_reference = index < server_runtime->pending_request.read_object_reference_count && server_runtime->pending_request.read_attribute_references[index][0] != '\0'
            ? server_runtime->pending_request.read_attribute_references[index]
            : server_runtime->pending_request.attribute_reference;

        if (!server_runtime_build_read_response_value(
                server_runtime,
                current_object_reference,
                value_bytes,
                sizeof(value_bytes),
                &value_length,
                &value_supported,
                diagnostic)) {
            return 0;
        }
        if (index == 0U) {
            server_runtime_store_read_summary(server_runtime, invoke_id, value_supported, value_bytes, value_length);
        }
        if (value_supported) {
            if (value_length > sizeof(access_result_value_bytes)) {
                server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Read response accessResult value is too small.");
                return 0;
            }
            memcpy(access_result_value_bytes, value_bytes, value_length);
            access_result_value_length = value_length;
        }
        else if (!server_runtime_encode_ber_element(
                UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
                0,
                0U,
                value_bytes,
                value_length,
                access_result_value_bytes,
                sizeof(access_result_value_bytes),
                &access_result_value_length,
                diagnostic)) {
            return 0;
        }
        {
            UnitLabMmsBerElement logged_value_element;
            char value_tag_hex[32U];
            char printable_value[128U];
            size_t logged_consumed_length = 0U;

            value_tag_hex[0] = '\0';
            printable_value[0] = '\0';
            unitlab_mms_ber_element_init(&logged_value_element);
            if (value_supported && unitlab_mms_ber_read(&logged_value_element, value_bytes, value_length, &logged_consumed_length, diagnostic)) {
                (void)server_runtime_tag_to_hex(&logged_value_element.tag, value_tag_hex, sizeof(value_tag_hex));
                if (server_runtime_bytes_are_printable_ascii(logged_value_element.value_bytes, logged_value_element.value_length)) {
                    size_t printable_length = logged_value_element.value_length < sizeof(printable_value) - 1U ? logged_value_element.value_length : sizeof(printable_value) - 1U;
                    memcpy(printable_value, logged_value_element.value_bytes, printable_length);
                    printable_value[printable_length] = '\0';
                }
                if (printable_value[0] != '\0') {
                    printf(
                        "native-wire-server: read-result invoke=%u index=%zu object=%s target=%s status=success value-tag=%s value-length=%zu value-string=\"%s\"\n",
                        (unsigned)invoke_id,
                        index,
                        current_object_reference[0] != '\0' ? current_object_reference : "<none>",
                        current_attribute_reference != NULL && current_attribute_reference[0] != '\0' ? current_attribute_reference : "<none>",
                        value_tag_hex[0] != '\0' ? value_tag_hex : "<invalid>",
                        logged_value_element.value_length,
                        printable_value);
                } else {
                    printf(
                        "native-wire-server: read-result invoke=%u index=%zu object=%s target=%s status=success value-tag=%s value-length=%zu\n",
                        (unsigned)invoke_id,
                        index,
                        current_object_reference[0] != '\0' ? current_object_reference : "<none>",
                        current_attribute_reference != NULL && current_attribute_reference[0] != '\0' ? current_attribute_reference : "<none>",
                        value_tag_hex[0] != '\0' ? value_tag_hex : "<invalid>",
                        logged_value_element.value_length);
                }
            } else {
                printf(
                    "native-wire-server: read-result invoke=%u index=%zu object=%s target=%s status=failure\n",
                    (unsigned)invoke_id,
                    index,
                    current_object_reference[0] != '\0' ? current_object_reference : "<none>",
                    current_attribute_reference != NULL && current_attribute_reference[0] != '\0' ? current_attribute_reference : "<none>");
            }
            fflush(stdout);
        }
        if (!value_supported) {
            printf(
                "native-wire-server: read target unsupported invoke=%u object=%s target=%s\n",
                (unsigned)invoke_id,
                current_object_reference[0] != '\0' ? current_object_reference : "<none>",
                current_attribute_reference != NULL && current_attribute_reference[0] != '\0' ? current_attribute_reference : "<none>");
            fflush(stdout);
        }
        if (list_of_access_result_length + access_result_value_length > sizeof(list_of_access_result_bytes)) {
            server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Read response accessResult list is too small.");
            return 0;
        }
        memcpy(&list_of_access_result_bytes[list_of_access_result_length], access_result_value_bytes, access_result_value_length);
        list_of_access_result_length += access_result_value_length;
    }
    if (!server_runtime_encode_ber_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            1,
            1U,
            list_of_access_result_bytes,
            list_of_access_result_length,
            list_of_access_result_wrapper_bytes,
            sizeof(list_of_access_result_wrapper_bytes),
            &list_of_access_result_wrapper_length,
            diagnostic)) {
        return 0;
    }
    if (!server_runtime_encode_ber_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            1,
            4U,
            list_of_access_result_wrapper_bytes,
            list_of_access_result_wrapper_length,
            read_response_body_bytes,
            sizeof(read_response_body_bytes),
            &read_response_body_length,
            diagnostic)) {
        return 0;
    }
    if (!server_runtime_encode_invoke_id_element(
            invoke_id,
            invoke_id_element_bytes,
            sizeof(invoke_id_element_bytes),
            &invoke_id_length,
            diagnostic)) {
        return 0;
    }

    if (invoke_id_length + read_response_body_length > sizeof(service_bytes)) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Read response service buffer is too small.");
        return 0;
    }
    memcpy(service_bytes, invoke_id_element_bytes, invoke_id_length);
    memcpy(service_bytes + invoke_id_length, read_response_body_bytes, read_response_body_length);
    total_length = invoke_id_length + read_response_body_length;
    if (total_length > buffer_length) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Read response service buffer is too small.");
        return 0;
    }
    memcpy(buffer, service_bytes, total_length);
    *encoded_length = total_length;
    server_runtime_log_read_response_tree(invoke_id, service_bytes, total_length, diagnostic);
    server_runtime_store_read_summary(server_runtime, invoke_id, value_supported, value_bytes, value_length);
    return 1;
}

