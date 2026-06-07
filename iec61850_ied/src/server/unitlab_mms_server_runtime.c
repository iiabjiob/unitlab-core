#include "unitlab_mms_server_runtime.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "wire/orchestration/unitlab_mms_wire_builder.h"
#include "wire/ber/unitlab_mms_ber.h"
#include "wire/acse/unitlab_mms_acse.h"
#include "wire/presentation/unitlab_mms_presentation.h"
#include "wire/session/unitlab_mms_session_spdu.h"
#include "wire/transport/unitlab_mms_transport_frame.h"
#include "protocols/mms/unitlab_mms_runtime_bridge.h"
#include "model/model_plan.h"

static void server_runtime_set_diagnostic(UnitLabMmsDiagnostic* diagnostic, UnitLabMmsDiagnosticCode code, const char* message)
{
    if (diagnostic == NULL) {
        return;
    }
    diagnostic->code = code;
    if (message == NULL) {
        diagnostic->message[0] = '\0';
        return;
    }
    snprintf(diagnostic->message, sizeof(diagnostic->message), "%s", message);
}

static int server_runtime_validate_config(const UnitLabIedServerConfig* config, UnitLabMmsDiagnostic* diagnostic)
{
    if (config == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Server config is required.");
        return 0;
    }
    if (config->bind_address == NULL || config->bind_address[0] == '\0') {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Server bind address is required.");
        return 0;
    }
    if (config->port <= 0 || config->port > 65535) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Server port must be in range 1..65535.");
        return 0;
    }
    return 1;
}

static void server_runtime_fail_and_capture(UnitLabMmsServerRuntime* server_runtime, UnitLabMmsOperationResult* operation_result)
{
    if (server_runtime == NULL || operation_result == NULL) {
        return;
    }
    server_runtime->last_result = *operation_result;
    unitlab_mms_server_runtime_capture_snapshot(server_runtime);
}

static const char* server_runtime_service_name_for_kind(UnitLabMmsServiceKind service_kind)
{
    switch (service_kind) {
        case UNITLAB_MMS_SERVICE_READ:
            return "Read";
        case UNITLAB_MMS_SERVICE_WRITE:
            return "Write";
        case UNITLAB_MMS_SERVICE_INFORMATION_REPORT:
            return "InformationReport";
        case UNITLAB_MMS_SERVICE_GET_NAME_LIST:
            return "GetNameList";
        case UNITLAB_MMS_SERVICE_GET_VARIABLE_ACCESS_ATTRIBUTES:
            return "GetVariableAccessAttributes";
        case UNITLAB_MMS_SERVICE_GET_NAMED_VARIABLE_LIST_ATTRIBUTES:
            return "GetNamedVariableListAttributes";
        case UNITLAB_MMS_SERVICE_RAW:
            return "Raw";
        case UNITLAB_MMS_SERVICE_NONE:
        default:
            return "<unknown>";
    }
}

static const char* server_runtime_decoded_service_name_for_pdu(const UnitLabMmsPdu* wire_pdu)
{
    if (wire_pdu == NULL) {
        return "<unknown>";
    }
    return server_runtime_service_name_for_kind(wire_pdu->service_kind);
}

static int server_runtime_tag_to_hex(const UnitLabMmsBerTag* tag, char* buffer, size_t buffer_length)
{
    uint8_t tag_bytes[8U];
    size_t encoded_length = 0U;
    size_t offset = 0U;
    UnitLabMmsDiagnostic diagnostic;

    if (buffer == NULL || buffer_length == 0U || tag == NULL) {
        return 0;
    }
    unitlab_mms_diagnostic_clear(&diagnostic);
    if (!unitlab_mms_ber_tag_encode(tag, tag_bytes, sizeof(tag_bytes), &encoded_length, &diagnostic)) {
        return 0;
    }
    if (encoded_length == 0U || (encoded_length * 2U) + 1U > buffer_length) {
        return 0;
    }
    for (size_t index = 0U; index < encoded_length; index++) {
        static const char hex_digits[] = "0123456789ABCDEF";
        buffer[offset++] = hex_digits[(tag_bytes[index] >> 4) & 0x0FU];
        buffer[offset++] = hex_digits[tag_bytes[index] & 0x0FU];
    }
    buffer[offset] = '\0';
    return 1;
}

static int server_runtime_bytes_are_printable_ascii(const uint8_t* bytes, size_t length)
{
    if (bytes == NULL) {
        return 0;
    }
    for (size_t index = 0U; index < length; index++) {
        uint8_t byte = bytes[index];
        if (byte < 0x20U || byte > 0x7EU) {
            return 0;
        }
    }
    return 1;
}

static void server_runtime_print_hex_bytes(const uint8_t* bytes, size_t length)
{
    static const char hex_digits[] = "0123456789ABCDEF";

    if (bytes == NULL || length == 0U) {
        printf("<empty>");
        return;
    }
    for (size_t index = 0U; index < length; index++) {
        putchar(hex_digits[(bytes[index] >> 4) & 0x0FU]);
        putchar(hex_digits[bytes[index] & 0x0FU]);
    }
}

static void server_runtime_store_incoming_context(UnitLabMmsServerRuntime* server_runtime, uint32_t invoke_id, const char* service_name)
{
    if (server_runtime == NULL) {
        return;
    }
    server_runtime->last_incoming_invoke_id = invoke_id;
    snprintf(server_runtime->last_incoming_service, sizeof(server_runtime->last_incoming_service), "%s", service_name != NULL && service_name[0] != '\0' ? service_name : "<unknown>");
}

static void server_runtime_store_outgoing_context(UnitLabMmsServerRuntime* server_runtime, uint32_t invoke_id, const char* service_name, const char* summary)
{
    if (server_runtime == NULL) {
        return;
    }
    server_runtime->last_outgoing_invoke_id = invoke_id;
    snprintf(server_runtime->last_outgoing_service, sizeof(server_runtime->last_outgoing_service), "%s", service_name != NULL && service_name[0] != '\0' ? service_name : "<unknown>");
    snprintf(server_runtime->last_outgoing_summary, sizeof(server_runtime->last_outgoing_summary), "%s", summary != NULL && summary[0] != '\0' ? summary : "<none>");
}

static void server_runtime_log_confirmed_request(UnitLabMmsServerRuntime* server_runtime, const UnitLabMmsPdu* wire_pdu)
{
    UnitLabMmsSemanticResult semantic_result;
    UnitLabMmsDecodeDiagnostic decode_diagnostic;
    char tag_hex[32U];
    const char* service_name = NULL;

    if (server_runtime == NULL || wire_pdu == NULL || wire_pdu->kind != UNITLAB_MMS_PDU_CONFIRMED_REQUEST) {
        return;
    }
    unitlab_mms_semantic_result_init(&semantic_result);
    unitlab_mms_decode_diagnostic_init(&decode_diagnostic);
    tag_hex[0] = '\0';
    (void)server_runtime_tag_to_hex(&wire_pdu->service_tag, tag_hex, sizeof(tag_hex));
    service_name = server_runtime_decoded_service_name_for_pdu(wire_pdu);
    if (unitlab_mms_semantic_result_from_wire_pdu(&semantic_result, wire_pdu, &decode_diagnostic)) {
        const char* object_reference = semantic_result.pdu.object_reference[0] != '\0' ? semantic_result.pdu.object_reference : "<none>";
        const char* domain_id = semantic_result.pdu.domain_id[0] != '\0' ? semantic_result.pdu.domain_id : "<none>";
        const char* item_id = semantic_result.pdu.item_id[0] != '\0'
            ? semantic_result.pdu.item_id
            : (semantic_result.pdu.continue_after[0] != '\0' ? semantic_result.pdu.continue_after : "<none>");
        printf(
            "native-wire-server: incoming-confirmed-request invoke=%u service-tag=%s service=%s object=%s domain=%s item=%s raw-mms-pdu-hex=",
            (unsigned)semantic_result.pdu.invoke_id,
            tag_hex[0] != '\0' ? tag_hex : "<invalid>",
            service_name,
            object_reference,
            domain_id,
            item_id);
    } else {
        printf(
            "native-wire-server: incoming-confirmed-request invoke=%u service-tag=%s service=%s object=<decode-failed> domain=<decode-failed> item=<decode-failed> raw-mms-pdu-hex=",
            (unsigned)(wire_pdu->has_invoke_id ? wire_pdu->invoke_id : 0U),
            tag_hex[0] != '\0' ? tag_hex : "<invalid>",
            service_name);
    }
    server_runtime_print_hex_bytes(wire_pdu->pdu_bytes, wire_pdu->pdu_length);
    putchar('\n');
    fflush(stdout);
    server_runtime_store_incoming_context(server_runtime, wire_pdu->has_invoke_id ? wire_pdu->invoke_id : 0U, service_name);
}

static void server_runtime_store_read_summary(UnitLabMmsServerRuntime* server_runtime, uint32_t invoke_id, int value_supported, const uint8_t* value_bytes, size_t value_length)
{
    UnitLabMmsBerElement value_element;
    UnitLabMmsDiagnostic diagnostic;
    size_t consumed_length = 0U;
    char summary[512U];

    if (server_runtime == NULL) {
        return;
    }
    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_ber_element_init(&value_element);
    summary[0] = '\0';
    if (value_bytes != NULL && value_length > 0U && unitlab_mms_ber_read(&value_element, value_bytes, value_length, &consumed_length, &diagnostic)) {
        char value_tag_hex[32U];
        char printable_value[128U];
        size_t printable_length = value_element.value_length < sizeof(printable_value) - 1U ? value_element.value_length : sizeof(printable_value) - 1U;

        value_tag_hex[0] = '\0';
        (void)server_runtime_tag_to_hex(&value_element.tag, value_tag_hex, sizeof(value_tag_hex));
        if (server_runtime_bytes_are_printable_ascii(value_element.value_bytes, value_element.value_length)) {
            memcpy(printable_value, value_element.value_bytes, printable_length);
            printable_value[printable_length] = '\0';
            snprintf(summary, sizeof(summary), "status=%s value-tag=%s value-length=%zu value-string=\"%s\"",
                value_supported ? "success" : "failure",
                value_tag_hex[0] != '\0' ? value_tag_hex : "<invalid>",
                value_element.value_length,
                printable_value);
        } else {
            snprintf(summary, sizeof(summary), "status=%s value-tag=%s value-length=%zu",
                value_supported ? "success" : "failure",
                value_tag_hex[0] != '\0' ? value_tag_hex : "<invalid>",
                value_element.value_length);
        }
    } else {
        snprintf(summary, sizeof(summary), "status=%s value-tag=<decode-failed> value-length=%zu",
            value_supported ? "success" : "failure",
            value_length);
    }
    server_runtime_store_outgoing_context(server_runtime, invoke_id, "Read", summary);
}

static void server_runtime_store_name_list_summary(UnitLabMmsServerRuntime* server_runtime, uint32_t invoke_id, const char* service_name, const char* label, const char* const* names, size_t name_count)
{
    char summary[512U];
    size_t offset = 0U;

    if (server_runtime == NULL) {
        return;
    }
    offset = (size_t)snprintf(summary, sizeof(summary), "%s=%zu list=[", label, name_count);
    for (size_t index = 0U; index < name_count && offset < sizeof(summary); index++) {
        const char* entry = names != NULL && names[index] != NULL ? names[index] : "<null>";
        int written = snprintf(&summary[offset], sizeof(summary) - offset, "%s%s", index > 0U ? "," : "", entry);
        if (written < 0) {
            summary[0] = '\0';
            break;
        }
        if ((size_t)written >= sizeof(summary) - offset) {
            offset = sizeof(summary) - 1U;
            break;
        }
        offset += (size_t)written;
    }
    if (offset < sizeof(summary) - 1U) {
        snprintf(&summary[offset], sizeof(summary) - offset, "]");
    }
    server_runtime_store_outgoing_context(server_runtime, invoke_id, service_name, summary);
}

static void server_runtime_log_confirmed_response_preview(UnitLabMmsServerRuntime* server_runtime, uint32_t invoke_id, const char* service_name, const uint8_t* service_bytes, size_t service_length)
{
    static const char hex_digits[] = "0123456789ABCDEF";
    size_t preview_length = service_length < 16U ? service_length : 16U;

    if (server_runtime == NULL) {
        return;
    }
    if (service_name == NULL) {
        service_name = "<unknown>";
    }
    printf("native-wire-server: confirmed-response invoke=%u service=%s preview-hex=", (unsigned)invoke_id, service_name);
    for (size_t index = 0U; index < preview_length; index++) {
        uint8_t byte = service_bytes != NULL ? service_bytes[index] : 0U;
        putchar(hex_digits[(byte >> 4) & 0x0FU]);
        putchar(hex_digits[byte & 0x0FU]);
    }
    printf(" summary=%s\n", server_runtime->last_outgoing_summary[0] != '\0' ? server_runtime->last_outgoing_summary : "<none>");
    fflush(stdout);
}

static int server_runtime_validate_confirmed_response_payload(const char* service_name, const uint8_t* service_bytes, size_t service_length, UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsBerElement invoke_id_element;
    UnitLabMmsBerElement response_element;
    size_t consumed_length = 0U;
    size_t response_consumed_length = 0U;

    if (service_bytes == NULL || service_length == 0U) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "Confirmed response payload is empty.");
        return 0;
    }
    if (service_bytes[0] != 0x02U) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "Confirmed response payload must start with invokeID INTEGER.");
        return 0;
    }
    unitlab_mms_ber_element_init(&invoke_id_element);
    if (!unitlab_mms_ber_read(&invoke_id_element, service_bytes, service_length, &consumed_length, diagnostic)) {
        return 0;
    }
    if (invoke_id_element.tag.tag_class != UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL || invoke_id_element.tag.tag_number != 2U || invoke_id_element.tag.constructed) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "Confirmed response payload must start with an INTEGER invokeID.");
        return 0;
    }
    if (consumed_length >= service_length) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "Confirmed response payload is missing confirmed service response.");
        return 0;
    }
    unitlab_mms_ber_element_init(&response_element);
    if (!unitlab_mms_ber_read(&response_element, &service_bytes[consumed_length], service_length - consumed_length, &response_consumed_length, diagnostic)) {
        return 0;
    }
    if (response_element.tag.tag_class != UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "Confirmed response payload must use a context-specific service response tag.");
        return 0;
    }
    if (response_consumed_length != service_length - consumed_length) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "Confirmed response payload contains trailing bytes.");
        return 0;
    }
    (void)service_name;
    return 1;
}

static int server_runtime_encode_ber_element(
    UnitLabMmsBerTagClass tag_class,
    int constructed,
    uint32_t tag_number,
    const uint8_t* value_bytes,
    size_t value_length,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsBerElement element;

    unitlab_mms_ber_element_init(&element);
    element.tag.tag_class = tag_class;
    element.tag.constructed = constructed;
    element.tag.tag_number = tag_number;
    element.value_bytes = value_bytes;
    element.value_length = value_length;
    return unitlab_mms_ber_write(&element, buffer, buffer_length, encoded_length, diagnostic);
}

static int server_runtime_encode_invoke_id_element(
    uint32_t invoke_id,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    uint8_t invoke_id_bytes[5U];
    size_t invoke_id_length_bytes = 0U;
    uint32_t value = invoke_id;
    UnitLabMmsBerElement invoke_id_element;

    do {
        invoke_id_bytes[sizeof(invoke_id_bytes) - 1U - invoke_id_length_bytes] = (uint8_t)(value & 0xFFU);
        invoke_id_length_bytes++;
        value >>= 8U;
    } while (value != 0U && invoke_id_length_bytes < sizeof(invoke_id_bytes));

    if (invoke_id_bytes[sizeof(invoke_id_bytes) - invoke_id_length_bytes] & 0x80U) {
        if (sizeof(invoke_id_bytes) == invoke_id_length_bytes) {
            server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Confirmed response invokeID encoding failed.");
            return 0;
        }
        invoke_id_bytes[sizeof(invoke_id_bytes) - invoke_id_length_bytes - 1U] = 0x00U;
        invoke_id_length_bytes++;
    }

    unitlab_mms_ber_element_init(&invoke_id_element);
    invoke_id_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL;
    invoke_id_element.tag.constructed = 0;
    invoke_id_element.tag.tag_number = 2U;
    invoke_id_element.value_bytes = &invoke_id_bytes[sizeof(invoke_id_bytes) - invoke_id_length_bytes];
    invoke_id_element.value_length = invoke_id_length_bytes;
    return unitlab_mms_ber_write(&invoke_id_element, buffer, buffer_length, encoded_length, diagnostic);
}

static int server_runtime_encode_confirmed_error_invoke_id_element(
    uint32_t invoke_id,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    uint8_t invoke_id_bytes[5U];
    size_t invoke_id_length_bytes = 0U;
    uint32_t value = invoke_id;
    UnitLabMmsBerElement invoke_id_element;

    do {
        invoke_id_bytes[sizeof(invoke_id_bytes) - 1U - invoke_id_length_bytes] = (uint8_t)(value & 0xFFU);
        invoke_id_length_bytes++;
        value >>= 8U;
    } while (value != 0U && invoke_id_length_bytes < sizeof(invoke_id_bytes));

    if (invoke_id_bytes[sizeof(invoke_id_bytes) - invoke_id_length_bytes] & 0x80U) {
        if (sizeof(invoke_id_bytes) == invoke_id_length_bytes) {
            server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Confirmed error invokeID encoding failed.");
            return 0;
        }
        invoke_id_bytes[sizeof(invoke_id_bytes) - invoke_id_length_bytes - 1U] = 0x00U;
        invoke_id_length_bytes++;
    }

    unitlab_mms_ber_element_init(&invoke_id_element);
    invoke_id_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    invoke_id_element.tag.constructed = 0;
    invoke_id_element.tag.tag_number = 0U;
    invoke_id_element.value_bytes = &invoke_id_bytes[sizeof(invoke_id_bytes) - invoke_id_length_bytes];
    invoke_id_element.value_length = invoke_id_length_bytes;
    return unitlab_mms_ber_write(&invoke_id_element, buffer, buffer_length, encoded_length, diagnostic);
}

static int server_runtime_parse_object_reference(const char* object_reference, char* domain_id, size_t domain_id_size, char* item_id, size_t item_id_size)
{
    const char* first_dot = NULL;
    const char* item = NULL;
    size_t domain_length = 0U;
    size_t item_length = 0U;

    if (domain_id == NULL || item_id == NULL || domain_id_size == 0U || item_id_size == 0U) {
        return 0;
    }
    domain_id[0] = '\0';
    item_id[0] = '\0';
    if (object_reference == NULL || object_reference[0] == '\0') {
        return 0;
    }
    first_dot = strchr(object_reference, '.');
    if (first_dot == NULL) {
        item = object_reference;
    } else {
        domain_length = (size_t)(first_dot - object_reference);
        if (domain_length == 0U || domain_length >= domain_id_size) {
            return 0;
        }
        memcpy(domain_id, object_reference, domain_length);
        domain_id[domain_length] = '\0';
        item = first_dot + 1;
    }
    item_length = strlen(item);
    if (item_length == 0U || item_length >= item_id_size) {
        return 0;
    }
    memcpy(item_id, item, item_length + 1U);
    return 1;
}

static const UnitLabIedModelSignal* server_runtime_find_signal_by_object_reference(const UnitLabMmsServerRuntime* server_runtime, const char* object_reference)
{
    if (server_runtime == NULL || server_runtime->model_plan == NULL || object_reference == NULL || object_reference[0] == '\0') {
        return NULL;
    }
    if (server_runtime->model_plan->signals == NULL || server_runtime->model_plan->signal_count == 0U) {
        return NULL;
    }
    for (size_t index = 0U; index < server_runtime->model_plan->signal_count; index++) {
        const UnitLabIedModelSignal* signal = &server_runtime->model_plan->signals[index];
        const char* suffix = object_reference;

        while (suffix != NULL && suffix[0] != '\0') {
            if (strcmp(signal->object_reference, object_reference) == 0 || strcmp(signal->object_reference, suffix) == 0) {
                return signal;
            }
            suffix = strchr(suffix, '.');
            if (suffix != NULL) {
                suffix++;
            }
        }
    }
    return NULL;
}

static int server_runtime_parse_int32_value(const char* source, int32_t* value)
{
    char* end = NULL;
    long parsed = strtol(source, &end, 10);
    if (source == NULL || value == NULL || source == end || end == NULL || *end != '\0' || parsed < INT32_MIN || parsed > INT32_MAX) {
        return 0;
    }
    *value = (int32_t)parsed;
    return 1;
}

static int server_runtime_encode_signed_integer(int32_t value, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic)
{
    uint8_t bytes[5U];
    size_t length = 0U;
    uint32_t raw = (uint32_t)value;
    int negative = value < 0;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    do {
        bytes[sizeof(bytes) - 1U - length] = (uint8_t)(raw & 0xFFU);
        length++;
        raw >>= 8U;
    } while (raw != 0U && raw != UINT32_MAX && length < sizeof(bytes));

    if (negative) {
        while (length < sizeof(bytes) && (bytes[sizeof(bytes) - length] & 0x80U) == 0U) {
            bytes[sizeof(bytes) - length - 1U] = 0xFFU;
            length++;
        }
    } else if (bytes[sizeof(bytes) - length] & 0x80U) {
        if (length == sizeof(bytes)) {
            server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Integer encoding is too large.");
            return 0;
        }
        bytes[sizeof(bytes) - length - 1U] = 0x00U;
        length++;
    }

    if (buffer_length < length) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Integer encoding buffer is too small.");
        return 0;
    }
    memcpy(buffer, &bytes[sizeof(bytes) - length], length);
    *encoded_length = length;
    return 1;
}

static int server_runtime_encode_mms_data_value(
    const UnitLabIedModelSignal* signal,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsBerElement element;
    int32_t integer_value = 0;
    uint8_t integer_bytes[5U];
    size_t integer_length = 0U;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (signal == NULL || buffer == NULL || encoded_length == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Read response value encoding requires a signal and output buffer.");
        return 0;
    }

    unitlab_mms_ber_element_init(&element);
    switch (signal->initial_value_kind) {
        case UNITLAB_IED_FIXTURE_VALUE_BOOLEAN: {
            uint8_t boolean_value;

            if (strcmp(signal->initial_value, "true") == 0) {
                boolean_value = 0xFFU;
            } else if (strcmp(signal->initial_value, "false") == 0) {
                boolean_value = 0x00U;
            } else {
                server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "Boolean read response value is invalid.");
                return 0;
            }
            element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL;
            element.tag.constructed = 0;
            element.tag.tag_number = 1U;
            element.value_bytes = &boolean_value;
            element.value_length = 1U;
            return unitlab_mms_ber_write(&element, buffer, buffer_length, encoded_length, diagnostic);
        }
        case UNITLAB_IED_FIXTURE_VALUE_INTEGER:
            if (!server_runtime_parse_int32_value(signal->initial_value, &integer_value)) {
                server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "Integer read response value is invalid.");
                return 0;
            }
            if (!server_runtime_encode_signed_integer(integer_value, integer_bytes, sizeof(integer_bytes), &integer_length, diagnostic)) {
                return 0;
            }
            element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL;
            element.tag.constructed = 0;
            element.tag.tag_number = 2U;
            element.value_bytes = &integer_bytes[sizeof(integer_bytes) - integer_length];
            element.value_length = integer_length;
            return unitlab_mms_ber_write(&element, buffer, buffer_length, encoded_length, diagnostic);
        case UNITLAB_IED_FIXTURE_VALUE_STRING:
            element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
            element.tag.constructed = 0;
            element.tag.tag_number = 10U;
            element.value_bytes = (const uint8_t*)signal->initial_value;
            element.value_length = strlen(signal->initial_value);
            return unitlab_mms_ber_write(&element, buffer, buffer_length, encoded_length, diagnostic);
        case UNITLAB_IED_FIXTURE_VALUE_NULL:
            element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL;
            element.tag.constructed = 0;
            element.tag.tag_number = 5U;
            element.value_bytes = NULL;
            element.value_length = 0U;
            return unitlab_mms_ber_write(&element, buffer, buffer_length, encoded_length, diagnostic);
        case UNITLAB_IED_FIXTURE_VALUE_REAL:
            server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "Real read response values are not yet supported by the native wire server.");
            return 0;
        case UNITLAB_IED_FIXTURE_VALUE_UNKNOWN:
        default:
            server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "Read response value kind is unsupported.");
            return 0;
    }
}

static int server_runtime_build_get_name_list_response_service(
    UnitLabMmsServerRuntime* server_runtime,
    uint32_t invoke_id,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    char** names = NULL;
    size_t name_count = 0U;
    uint8_t visible_strings_bytes[2048U];
    uint8_t list_of_identifier_bytes[2048U];
    uint8_t get_name_list_body_bytes[2048U];
    uint8_t service_payload_bytes[2048U];
    uint8_t service_bytes[2048U];
    size_t visible_strings_length = 0U;
    size_t list_of_identifier_length = 0U;
    size_t get_name_list_body_length = 0U;
    size_t service_payload_length = 0U;
    size_t invoke_id_length = 0U;
    size_t total_length = 0U;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (buffer == NULL || encoded_length == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "GetNameList response buffer and encoded_length are required.");
        return 0;
    }
    if (!unitlab_mms_pending_request_collect_get_name_list_names(&server_runtime->pending_request, server_runtime->model_plan, &names, &name_count, diagnostic)) {
        return 0;
    }
    const char* browse_semantics = "GetNameList(UNSUPPORTED)";

    if (server_runtime->pending_request.browse_object_class == 9U && server_runtime->pending_request.browse_object_scope == 0U) {
        browse_semantics = "GetNameList(VMD-SPECIFIC)";
    } else if (server_runtime->pending_request.browse_object_class == 0U && server_runtime->pending_request.browse_object_scope == 1U) {
        browse_semantics = "GetNameList(DOMAIN-SPECIFIC)";
    } else if (server_runtime->pending_request.browse_object_class == 2U && server_runtime->pending_request.browse_object_scope == 0U) {
        browse_semantics = "GetNameList(VMD-NVL)";
    } else if (server_runtime->pending_request.browse_object_class == 2U && server_runtime->pending_request.browse_object_scope == 1U) {
        browse_semantics = "GetNameList(DATASET-SPECIFIC)";
    } else if (server_runtime->pending_request.browse_object_class == 2U && server_runtime->pending_request.browse_object_scope == 2U) {
        browse_semantics = "GetNameList(AA-SPECIFIC)";
    } else if ((server_runtime->pending_request.browse_object_class == 1U || server_runtime->pending_request.browse_object_class == 3U) && server_runtime->pending_request.browse_object_scope == 1U) {
        browse_semantics = "GetNameList(LOGICAL-NODE-SPECIFIC)";
    } else if (server_runtime->pending_request.browse_object_class == 4U && server_runtime->pending_request.browse_object_scope == 1U) {
        browse_semantics = "GetNameList(BUFFERED-REPORT)";
    } else if (server_runtime->pending_request.browse_object_class == 5U && server_runtime->pending_request.browse_object_scope == 1U) {
        browse_semantics = "GetNameList(UNBUFFERED-REPORT)";
    }

    printf(
        "native-wire-server: confirmed-response invoke=%u service=GetNameList semantic=%s browse-class=%u browse-scope=%u domain=%s continue-after=%s identifiers=%zu moreFollows=false\n",
        (unsigned)invoke_id,
        browse_semantics,
        (unsigned)server_runtime->pending_request.browse_object_class,
        (unsigned)server_runtime->pending_request.browse_object_scope,
        server_runtime->pending_request.browse_domain_id[0] != '\0' ? server_runtime->pending_request.browse_domain_id : "<none>",
        server_runtime->pending_request.browse_continue_after[0] != '\0' ? server_runtime->pending_request.browse_continue_after : "<none>",
        name_count);
    fflush(stdout);
    for (size_t index = 0U; index < name_count; index++) {
        size_t encoded_name_length = 0U;

        if (!server_runtime_encode_ber_element(
                UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL,
                0,
                26U,
                (const uint8_t*)names[index],
                strlen(names[index]),
                &visible_strings_bytes[visible_strings_length],
                sizeof(visible_strings_bytes) - visible_strings_length,
                &encoded_name_length,
                diagnostic)) {
            unitlab_free_ied_model_name_list(names, name_count);
            return 0;
        }
        visible_strings_length += encoded_name_length;
    }
    if (!server_runtime_encode_ber_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            1,
            0U,
            visible_strings_bytes,
            visible_strings_length,
            list_of_identifier_bytes,
            sizeof(list_of_identifier_bytes),
            &list_of_identifier_length,
            diagnostic)) {
        unitlab_free_ied_model_name_list(names, name_count);
        return 0;
    }
    memcpy(get_name_list_body_bytes, list_of_identifier_bytes, list_of_identifier_length);
    {
        uint8_t more_follows_bytes[1U] = { 0x00U };
        size_t more_follows_length = 0U;

        if (!server_runtime_encode_ber_element(
                UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
                0,
                1U,
                more_follows_bytes,
                sizeof(more_follows_bytes),
                &get_name_list_body_bytes[list_of_identifier_length],
                sizeof(get_name_list_body_bytes) - list_of_identifier_length,
                &more_follows_length,
                diagnostic)) {
            unitlab_free_ied_model_name_list(names, name_count);
            return 0;
        }
        get_name_list_body_length = list_of_identifier_length + more_follows_length;
    }
    if (!server_runtime_encode_ber_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            1,
            1U,
            get_name_list_body_bytes,
            get_name_list_body_length,
            service_payload_bytes,
            sizeof(service_payload_bytes),
            &service_payload_length,
            diagnostic)) {
        unitlab_free_ied_model_name_list(names, name_count);
        return 0;
    }
    if (!server_runtime_encode_invoke_id_element(
            invoke_id,
            service_bytes,
            sizeof(service_bytes),
            &invoke_id_length,
            diagnostic)) {
        unitlab_free_ied_model_name_list(names, name_count);
        return 0;
    }
    if (invoke_id_length + service_payload_length > sizeof(service_bytes)) {
        unitlab_free_ied_model_name_list(names, name_count);
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "GetNameList response output buffer is too small.");
        return 0;
    }
    memcpy(&service_bytes[invoke_id_length], service_payload_bytes, service_payload_length);
    total_length = invoke_id_length + service_payload_length;
    if (total_length > buffer_length) {
        unitlab_free_ied_model_name_list(names, name_count);
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "GetNameList response output buffer is too small.");
        return 0;
    }
    memcpy(buffer, service_bytes, total_length);
    *encoded_length = total_length;
    server_runtime_store_name_list_summary(server_runtime, invoke_id, "GetNameList", "identifiers", (const char* const*)names, name_count);
    unitlab_free_ied_model_name_list(names, name_count);
    server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}


static const char* const lln0_mod_children[] = { "q", "t" };
static const char* const lln0_beh_children[] = { "stVal", "q", "t" };
static const char* const lln0_health_children[] = { "stVal", "q", "t" };
static const char* const lln0_cf_children[] = { "Mod" };
static const char* const lln0_cf_mod_children[] = { "ctlModel" };
static const char* const lln0_dc_children[] = { "NamPlt" };
static const char* const lln0_br_children[] = { "brcbEvents" };
static const char* const lln0_br_rcb_children[] = {
    "RptID",
    "RptEna",
    "DatSet",
    "ConfRev",
    "OptFlds",
    "BufTm",
    "SqNum",
    "TrgOps",
    "IntgPd",
    "GI",
    "PurgeBuf",
    "EntryID",
    "TimeOfEntry",
    "ResvTms",
    "Owner"
};
static const char* const lln0_ex_children[] = { "NamPlt" };
static const char* const lln0_ex_namplt_children[] = { "ldNs", "lnNs", "cdcNs", "dataNs" };
static const char* const lln0_namplt_children[] = { "vendor", "swRev", "d", "configRev" };

static int server_runtime_object_reference_has_suffix(const char* object_reference, const char* suffix);

static int server_runtime_object_reference_has_suffix_any(
    const char* object_reference,
    const char* const* suffixes,
    size_t suffix_count)
{
    if (object_reference == NULL || suffixes == NULL || suffix_count == 0U) {
        return 0;
    }
    for (size_t index = 0U; index < suffix_count; index++) {
        if (server_runtime_object_reference_has_suffix(object_reference, suffixes[index])) {
            return 1;
        }
    }
    return 0;
}


static int server_runtime_encode_gva_leaf_type_spec(
    const char* component_name,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsBerElement type_element;
    const uint8_t* value_bytes = NULL;
    size_t value_length = 0U;
    uint8_t value_q_or_bool[1U];
    uint8_t value_visible_2[2U];
    uint8_t value_single[1U];
    uint8_t tag_number = 3U;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (component_name == NULL || buffer == NULL || encoded_length == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "GVA leaf type encoding requires a component name, buffer, and encoded_length.");
        return 0;
    }

    if (strcmp(component_name, "q") == 0) {
        value_q_or_bool[0] = 0xF3U;
        tag_number = 4U;
        value_bytes = value_q_or_bool;
        value_length = sizeof(value_q_or_bool);
    }
    else if (strcmp(component_name, "t") == 0) {
        tag_number = 17U;
        value_bytes = NULL;
        value_length = 0U;
    }
    else if (strcmp(component_name, "stVal") == 0) {
        value_single[0] = 0x20U;
        tag_number = 5U;
        value_bytes = value_single;
        value_length = sizeof(value_single);
    }
    else if (strcmp(component_name, "ctlModel") == 0) {
        value_single[0] = 0x08U;
        tag_number = 5U;
        value_bytes = value_single;
        value_length = sizeof(value_single);
    }
    else if (strcmp(component_name, "d") == 0 || strcmp(component_name, "vendor") == 0 || strcmp(component_name, "swRev") == 0 || strcmp(component_name, "configRev") == 0) {
        value_visible_2[0] = 0xFFU;
        value_visible_2[1] = 0x01U;
        tag_number = 10U;
        value_bytes = value_visible_2;
        value_length = sizeof(value_visible_2);
    }
    else if (strcmp(component_name, "ldNs") == 0 || strcmp(component_name, "lnNs") == 0 || strcmp(component_name, "cdcNs") == 0 || strcmp(component_name, "dataNs") == 0) {
        value_visible_2[0] = 0x4EU;
        value_visible_2[1] = 0x53U;
        tag_number = 10U;
        value_bytes = value_visible_2;
        value_length = sizeof(value_visible_2);
    }
    else if (strcmp(component_name, "RptID") == 0 || strcmp(component_name, "DatSet") == 0) {
        value_visible_2[0] = 0xFFU;
        value_visible_2[1] = 0x7FU;
        tag_number = 10U;
        value_bytes = value_visible_2;
        value_length = sizeof(value_visible_2);
    }
    else if (strcmp(component_name, "ConfRev") == 0 || strcmp(component_name, "BufTm") == 0 || strcmp(component_name, "IntgPd") == 0) {
        value_single[0] = 0x20U;
        tag_number = 6U;
        value_bytes = value_single;
        value_length = sizeof(value_single);
    }
    else if (strcmp(component_name, "SqNum") == 0 || strcmp(component_name, "ResvTms") == 0) {
        value_single[0] = 0x10U;
        tag_number = 6U;
        value_bytes = value_single;
        value_length = sizeof(value_single);
    }
    else if (strcmp(component_name, "OptFlds") == 0) {
        value_single[0] = 0xF6U;
        tag_number = 4U;
        value_bytes = value_single;
        value_length = sizeof(value_single);
    }
    else if (strcmp(component_name, "TrgOps") == 0) {
        value_single[0] = 0xFAU;
        tag_number = 4U;
        value_bytes = value_single;
        value_length = sizeof(value_single);
    }
    else if (strcmp(component_name, "GI") == 0 || strcmp(component_name, "RptEna") == 0 || strcmp(component_name, "PurgeBuf") == 0) {
        tag_number = 3U;
        value_bytes = NULL;
        value_length = 0U;
    }
    else if (strcmp(component_name, "EntryID") == 0) {
        value_single[0] = 0x08U;
        tag_number = 9U;
        value_bytes = value_single;
        value_length = sizeof(value_single);
    }
    else if (strcmp(component_name, "TimeOfEntry") == 0 || strcmp(component_name, "TimeofEntry") == 0) {
        value_single[0] = 0x01U;
        tag_number = 12U;
        value_bytes = value_single;
        value_length = sizeof(value_single);
    }
    else if (strcmp(component_name, "Owner") == 0) {
        value_visible_2[0] = 0x4FU;
        value_visible_2[1] = 0x57U;
        tag_number = 10U;
        value_bytes = value_visible_2;
        value_length = sizeof(value_visible_2);
    }

    unitlab_mms_ber_element_init(&type_element);
    type_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    type_element.tag.constructed = 0;
    type_element.tag.tag_number = tag_number;
    type_element.value_bytes = value_bytes;
    type_element.value_length = value_length;
    if (!unitlab_mms_ber_write(&type_element, buffer, buffer_length, encoded_length, diagnostic)) {
        return 0;
    }
    return 1;
}

static const char* const* server_runtime_lookup_gva_children(
    const char* logical_node_name,
    const char* parent_component_name,
    const char* component_name,
    size_t* child_count)
{
    if (child_count != NULL) {
        *child_count = 0U;
    }
    if (logical_node_name == NULL || component_name == NULL || child_count == NULL) {
        return NULL;
    }

    if (strcmp(logical_node_name, "LLN0") != 0) {
        return NULL;
    }
    if (parent_component_name == NULL) {
        if (strcmp(component_name, "Mod") == 0) {
            *child_count = sizeof(lln0_mod_children) / sizeof(lln0_mod_children[0]);
            return lln0_mod_children;
        }
        if (strcmp(component_name, "Beh") == 0) {
            *child_count = sizeof(lln0_beh_children) / sizeof(lln0_beh_children[0]);
            return lln0_beh_children;
        }
        if (strcmp(component_name, "Health") == 0) {
            *child_count = sizeof(lln0_health_children) / sizeof(lln0_health_children[0]);
            return lln0_health_children;
        }
        if (strcmp(component_name, "CF") == 0) {
            *child_count = sizeof(lln0_cf_children) / sizeof(lln0_cf_children[0]);
            return lln0_cf_children;
        }
        if (strcmp(component_name, "DC") == 0) {
            *child_count = sizeof(lln0_dc_children) / sizeof(lln0_dc_children[0]);
            return lln0_dc_children;
        }
        if (strcmp(component_name, "NamPlt") == 0) {
            *child_count = sizeof(lln0_namplt_children) / sizeof(lln0_namplt_children[0]);
            return lln0_namplt_children;
        }
        if (strcmp(component_name, "EX") == 0) {
            *child_count = sizeof(lln0_ex_children) / sizeof(lln0_ex_children[0]);
            return lln0_ex_children;
        }
        if (strcmp(component_name, "BR") == 0) {
            *child_count = sizeof(lln0_br_children) / sizeof(lln0_br_children[0]);
            return lln0_br_children;
        }
    }
    else if (strcmp(parent_component_name, "CF") == 0 && strcmp(component_name, "Mod") == 0) {
        *child_count = sizeof(lln0_cf_mod_children) / sizeof(lln0_cf_mod_children[0]);
        return lln0_cf_mod_children;
    }
    else if (strcmp(parent_component_name, "DC") == 0 && strcmp(component_name, "NamPlt") == 0) {
        *child_count = sizeof(lln0_namplt_children) / sizeof(lln0_namplt_children[0]);
        return lln0_namplt_children;
    }
    else if (strcmp(parent_component_name, "BR") == 0 && strcmp(component_name, "brcbEvents") == 0) {
        *child_count = sizeof(lln0_br_rcb_children) / sizeof(lln0_br_rcb_children[0]);
        return lln0_br_rcb_children;
    }
    else if (strcmp(parent_component_name, "EX") == 0 && strcmp(component_name, "NamPlt") == 0) {
        *child_count = sizeof(lln0_ex_namplt_children) / sizeof(lln0_ex_namplt_children[0]);
        return lln0_ex_namplt_children;
    }
    return NULL;
}


static int server_runtime_encode_report_control_block_field_value(
    const char* field_name,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsBerElement value_element;
    uint8_t integer_bytes[5U];
    uint8_t value_bool_byte[1U];
    uint8_t value_octet_byte[1U];
    int32_t integer_value = 0;
    size_t integer_length = 0U;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (field_name == NULL || buffer == NULL || encoded_length == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Report control block field encoding requires a field name, buffer, and encoded_length.");
        return 0;
    }

    unitlab_mms_ber_element_init(&value_element);

    if (strcmp(field_name, "RptID") == 0) {
        value_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
        value_element.tag.constructed = 0;
        value_element.tag.tag_number = 10U;
        value_element.value_bytes = (const uint8_t*)"IED1LD0/LLN0.BR.Events";
        value_element.value_length = strlen((const char*)value_element.value_bytes);
    }
    else if (strcmp(field_name, "RptEna") == 0 || strcmp(field_name, "GI") == 0 || strcmp(field_name, "PurgeBuf") == 0) {
        value_bool_byte[0] = 0x00U;
        value_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL;
        value_element.tag.constructed = 0;
        value_element.tag.tag_number = 1U;
        value_element.value_bytes = value_bool_byte;
        value_element.value_length = sizeof(value_bool_byte);
    }
    else if (strcmp(field_name, "DatSet") == 0) {
        value_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
        value_element.tag.constructed = 0;
        value_element.tag.tag_number = 10U;
        value_element.value_bytes = (const uint8_t*)"LD0/LLN0$dsEvents";
        value_element.value_length = strlen((const char*)value_element.value_bytes);
    }
    else if (strcmp(field_name, "ConfRev") == 0) {
        integer_value = 7;
        if (!server_runtime_encode_signed_integer(integer_value, integer_bytes, sizeof(integer_bytes), &integer_length, diagnostic)) {
            return 0;
        }
        value_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL;
        value_element.tag.constructed = 0;
        value_element.tag.tag_number = 2U;
        value_element.value_bytes = &integer_bytes[sizeof(integer_bytes) - integer_length];
        value_element.value_length = integer_length;
    }
    else if (strcmp(field_name, "OptFlds") == 0) {
        value_octet_byte[0] = 0xF6U;
        value_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL;
        value_element.tag.constructed = 0;
        value_element.tag.tag_number = 4U;
        value_element.value_bytes = value_octet_byte;
        value_element.value_length = sizeof(value_octet_byte);
    }
    else if (strcmp(field_name, "BufTm") == 0) {
        integer_value = 100;
        if (!server_runtime_encode_signed_integer(integer_value, integer_bytes, sizeof(integer_bytes), &integer_length, diagnostic)) {
            return 0;
        }
        value_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL;
        value_element.tag.constructed = 0;
        value_element.tag.tag_number = 2U;
        value_element.value_bytes = &integer_bytes[sizeof(integer_bytes) - integer_length];
        value_element.value_length = integer_length;
    }
    else if (strcmp(field_name, "SqNum") == 0 || strcmp(field_name, "ResvTms") == 0) {
        integer_value = 0;
        if (!server_runtime_encode_signed_integer(integer_value, integer_bytes, sizeof(integer_bytes), &integer_length, diagnostic)) {
            return 0;
        }
        value_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL;
        value_element.tag.constructed = 0;
        value_element.tag.tag_number = 2U;
        value_element.value_bytes = &integer_bytes[sizeof(integer_bytes) - integer_length];
        value_element.value_length = integer_length;
    }
    else if (strcmp(field_name, "TrgOps") == 0) {
        value_octet_byte[0] = 0xFAU;
        value_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL;
        value_element.tag.constructed = 0;
        value_element.tag.tag_number = 4U;
        value_element.value_bytes = value_octet_byte;
        value_element.value_length = sizeof(value_octet_byte);
    }
    else if (strcmp(field_name, "IntgPd") == 0) {
        integer_value = 1000;
        if (!server_runtime_encode_signed_integer(integer_value, integer_bytes, sizeof(integer_bytes), &integer_length, diagnostic)) {
            return 0;
        }
        value_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL;
        value_element.tag.constructed = 0;
        value_element.tag.tag_number = 2U;
        value_element.value_bytes = &integer_bytes[sizeof(integer_bytes) - integer_length];
        value_element.value_length = integer_length;
    }
    else if (strcmp(field_name, "EntryID") == 0) {
        value_octet_byte[0] = 0x08U;
        value_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL;
        value_element.tag.constructed = 0;
        value_element.tag.tag_number = 9U;
        value_element.value_bytes = value_octet_byte;
        value_element.value_length = sizeof(value_octet_byte);
    }
    else if (strcmp(field_name, "TimeOfEntry") == 0) {
        value_octet_byte[0] = 0x01U;
        value_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL;
        value_element.tag.constructed = 0;
        value_element.tag.tag_number = 12U;
        value_element.value_bytes = value_octet_byte;
        value_element.value_length = sizeof(value_octet_byte);
    }
    else if (strcmp(field_name, "Owner") == 0) {
        value_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
        value_element.tag.constructed = 0;
        value_element.tag.tag_number = 10U;
        value_element.value_bytes = (const uint8_t*)"";
        value_element.value_length = 0U;
    }
    else {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "Unsupported report control block field.");
        return 0;
    }

    if (!unitlab_mms_ber_write(&value_element, buffer, buffer_length, encoded_length, diagnostic)) {
        return 0;
    }
    return 1;
}

static int server_runtime_encode_report_control_block_value(
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    static const char* const report_control_block_fields[] = {
        "RptID",
        "RptEna",
        "DatSet",
        "ConfRev",
        "OptFlds",
        "BufTm",
        "SqNum",
        "TrgOps",
        "IntgPd",
        "GI",
        "PurgeBuf",
        "EntryID",
        "TimeOfEntry",
        "ResvTms",
        "Owner"
    };
    uint8_t structure_bytes[2048U];
    size_t structure_length = 0U;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (buffer == NULL || encoded_length == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Report control block value encoding requires a buffer and encoded_length.");
        return 0;
    }

    for (size_t index = 0U; index < sizeof(report_control_block_fields) / sizeof(report_control_block_fields[0]); index++) {
        uint8_t field_bytes[128U];
        size_t field_length = 0U;

        if (!server_runtime_encode_report_control_block_field_value(
                report_control_block_fields[index],
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
        structure_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL;
        structure_element.tag.constructed = 1;
        structure_element.tag.tag_number = 16U;
        structure_element.value_bytes = structure_bytes;
        structure_element.value_length = structure_length;
        if (!unitlab_mms_ber_write(&structure_element, buffer, buffer_length, encoded_length, diagnostic)) {
            return 0;
        }
    }

    return 1;
}

static int server_runtime_copy_static_names(
    const char* const* source_names,
    size_t source_count,
    char*** names,
    size_t* count,
    UnitLabMmsDiagnostic* diagnostic)
{
    if (names == NULL || count == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Static GVA name copy requires output pointers.");
        return 0;
    }
    *names = NULL;
    *count = 0U;
    if (source_count == 0U) {
        return 1;
    }
    *names = (char**)calloc(source_count, sizeof(char*));
    if (*names == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Cannot allocate static GVA names.");
        return 0;
    }
    for (size_t index = 0U; index < source_count; index++) {
        size_t length = strlen(source_names[index]);

        (*names)[index] = (char*)calloc(length + 1U, sizeof(char));
        if ((*names)[index] == NULL) {
            unitlab_free_ied_model_name_list(*names, index);
            *names = NULL;
            *count = 0U;
            server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Cannot allocate static GVA name.");
            return 0;
        }
        memcpy((*names)[index], source_names[index], length + 1U);
        *count = index + 1U;
    }
    return 1;
}

static int server_runtime_encode_gva_component_tree(
    const char* logical_node_name,
    const char* parent_component_name,
    const char* component_name,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    uint8_t component_name_bytes[256U];
    uint8_t component_type_bytes[8192U];
    uint8_t component_components_wrapper_bytes[12288U];
    uint8_t component_structure_bytes[16384U];
    uint8_t component_type_wrapper_bytes[20480U];
    uint8_t component_content_bytes[16384U];
    uint8_t component_bytes[32768U];
    const char* const* child_names = NULL;
    size_t child_count = 0U;
    size_t component_name_length = 0U;
    size_t component_type_length = 0U;
    size_t component_components_wrapper_length = 0U;
    size_t component_structure_length = 0U;
    size_t component_type_wrapper_length = 0U;
    size_t component_content_length = 0U;
    size_t component_length = 0U;
    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (component_name == NULL || buffer == NULL || encoded_length == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "GVA component encoding requires a component name, buffer, and encoded_length.");
        return 0;
    }

    if (!server_runtime_encode_ber_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            0,
            0U,
            (const uint8_t*)component_name,
            strlen(component_name),
            component_name_bytes,
            sizeof(component_name_bytes),
            &component_name_length,
            diagnostic)) {
        return 0;
    }

    child_names = server_runtime_lookup_gva_children(logical_node_name, parent_component_name, component_name, &child_count);
    if (child_names != NULL && child_count > 0U) {
        size_t child_component_bytes_length = 0U;

        for (size_t child_index = 0U; child_index < child_count; child_index++) {
            size_t child_length = 0U;

            if (!server_runtime_encode_gva_component_tree(
                    logical_node_name,
                    component_name,
                    child_names[child_index],
                    &component_content_bytes[child_component_bytes_length],
                    sizeof(component_content_bytes) - child_component_bytes_length,
                    &child_length,
                    diagnostic)) {
                return 0;
            }
            child_component_bytes_length += child_length;
        }
        if (child_component_bytes_length > sizeof(component_type_bytes)) {
            server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "GVA component type buffer is too small.");
            return 0;
        }
        memcpy(component_type_bytes, component_content_bytes, child_component_bytes_length);
        component_type_length = child_component_bytes_length;
        if (!server_runtime_encode_ber_element(
                UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
                1,
                1U,
                component_type_bytes,
                component_type_length,
                component_components_wrapper_bytes,
                sizeof(component_components_wrapper_bytes),
                &component_components_wrapper_length,
                diagnostic)) {
            return 0;
        }
        if (!server_runtime_encode_ber_element(
                UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
                1,
                2U,
                component_components_wrapper_bytes,
                component_components_wrapper_length,
                component_structure_bytes,
                sizeof(component_structure_bytes),
                &component_structure_length,
                diagnostic)) {
            return 0;
        }
        memcpy(component_type_bytes, component_structure_bytes, component_structure_length);
        component_type_length = component_structure_length;
    } else {
        if (!server_runtime_encode_gva_leaf_type_spec(
                component_name,
                component_type_bytes,
                sizeof(component_type_bytes),
                &component_type_length,
                diagnostic)) {
            return 0;
        }
    }

    if (!server_runtime_encode_ber_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            1,
            1U,
            component_type_bytes,
            component_type_length,
            component_type_wrapper_bytes,
            sizeof(component_type_wrapper_bytes),
            &component_type_wrapper_length,
            diagnostic)) {
        return 0;
    }
    if (component_name_length + component_type_wrapper_length > sizeof(component_content_bytes)) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "GVA component encoding buffer is too small.");
        return 0;
    }
    memcpy(component_content_bytes, component_name_bytes, component_name_length);
    memcpy(&component_content_bytes[component_name_length], component_type_wrapper_bytes, component_type_wrapper_length);
    component_content_length = component_name_length + component_type_wrapper_length;
    if (!server_runtime_encode_ber_element(
            UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL,
            1,
            16U,
            component_content_bytes,
            component_content_length,
            component_bytes,
            sizeof(component_bytes),
            &component_length,
            diagnostic)) {
        return 0;
    }
    if (component_length > buffer_length) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "GVA component output buffer is too small.");
        return 0;
    }
    memcpy(buffer, component_bytes, component_length);
    *encoded_length = component_length;
    return 1;
}

static int server_runtime_build_get_variable_access_attributes_response_service(
    UnitLabMmsServerRuntime* server_runtime,
    uint32_t invoke_id,
    const char* object_reference,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    char domain_id[128U];
    char item_id[128U];
    char model_error[256U];
    char logical_node_for_gva[128U];
    const char* root_parent_component_name = NULL;
    char** names = NULL;
    size_t name_count = 0U;
    uint8_t component_bytes[4096U];
    uint8_t components_wrapper_bytes[4096U];
    uint8_t type_spec_bytes[4096U];
    uint8_t type_spec_wrapper_bytes[4096U];
    uint8_t response_payload_bytes[8192U];
    uint8_t service_payload_bytes[8192U];
    uint8_t service_bytes[8192U];
    size_t component_bytes_length = 0U;
    size_t components_wrapper_length = 0U;
    size_t type_spec_length = 0U;
    size_t type_spec_wrapper_length = 0U;
    size_t response_payload_length = 0U;
    size_t service_payload_length = 0U;
    size_t invoke_id_length = 0U;
    size_t total_length = 0U;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (server_runtime == NULL || buffer == NULL || encoded_length == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "GVA response buffer and encoded_length are required.");
        return 0;
    }
    if (object_reference == NULL || object_reference[0] == '\0') {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "GVA response requires an object reference.");
        return 0;
    }
    if (!server_runtime_parse_object_reference(object_reference, domain_id, sizeof(domain_id), item_id, sizeof(item_id))) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "GVA response object reference is malformed.");
        return 0;
    }

    model_error[0] = '\0';
    snprintf(logical_node_for_gva, sizeof(logical_node_for_gva), "%s", item_id);
    if (strcmp(item_id, "LLN0$BR") == 0 || strcmp(item_id, "LLN0.BR") == 0) {
        snprintf(logical_node_for_gva, sizeof(logical_node_for_gva), "%s", "LLN0");
        root_parent_component_name = "BR";
        if (!server_runtime_copy_static_names(lln0_br_children, sizeof(lln0_br_children) / sizeof(lln0_br_children[0]), &names, &name_count, diagnostic)) {
            return 0;
        }
    }
    else if (strcmp(item_id, "LLN0$BR$brcbEvents") == 0 || strcmp(item_id, "LLN0.BR.brcbEvents") == 0) {
        snprintf(logical_node_for_gva, sizeof(logical_node_for_gva), "%s", "LLN0");
        if (!server_runtime_copy_static_names(lln0_br_rcb_children, sizeof(lln0_br_rcb_children) / sizeof(lln0_br_rcb_children[0]), &names, &name_count, diagnostic)) {
            return 0;
        }
    }
    else if (domain_id[0] != '\0') {
        if (!unitlab_collect_ied_model_logical_node_variables(
                server_runtime->model_plan,
                domain_id,
                item_id,
                &names,
                &name_count,
                model_error,
                sizeof(model_error))) {
            unitlab_free_ied_model_name_list(names, name_count);
            server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, model_error[0] != '\0' ? model_error : "GVA response lookup failed.");
            return 0;
        }
    } else {
        if (!unitlab_collect_ied_model_vmd_named_variable_lists(
                server_runtime->model_plan,
                &names,
                &name_count,
                model_error,
                sizeof(model_error))) {
            unitlab_free_ied_model_name_list(names, name_count);
            server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, model_error[0] != '\0' ? model_error : "GVA response lookup failed.");
            return 0;
        }
    }

    printf(
        "native-wire-server: confirmed-response invoke=%u service=GetVariableAccessAttributes object=%s attribute=%s type-kind=structure components=%zu\n",
        (unsigned)invoke_id,
        object_reference,
        item_id,
        name_count);
    for (size_t index = 0U; index < name_count; index++) {
        printf("native-wire-server: gva-component[%zu]=%s\n", index, names[index]);
    }
    fflush(stdout);
    for (size_t index = 0U; index < name_count; index++) {
        size_t component_length = 0U;

        if (!server_runtime_encode_gva_component_tree(
                logical_node_for_gva,
                root_parent_component_name,
                names[index],
                &component_bytes[component_bytes_length],
                sizeof(component_bytes) - component_bytes_length,
                &component_length,
                diagnostic)) {
            unitlab_free_ied_model_name_list(names, name_count);
            return 0;
        }
        component_bytes_length += component_length;
    }

    if (!server_runtime_encode_ber_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            1,
            1U,
            component_bytes,
            component_bytes_length,
            components_wrapper_bytes,
            sizeof(components_wrapper_bytes),
            &components_wrapper_length,
            diagnostic)) {
        unitlab_free_ied_model_name_list(names, name_count);
        return 0;
    }

    if (!server_runtime_encode_ber_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            1,
            2U,
            components_wrapper_bytes,
            components_wrapper_length,
            type_spec_bytes,
            sizeof(type_spec_bytes),
            &type_spec_length,
            diagnostic)) {
        unitlab_free_ied_model_name_list(names, name_count);
        return 0;
    }

    if (!server_runtime_encode_ber_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            1,
            2U,
            type_spec_bytes,
            type_spec_length,
            type_spec_wrapper_bytes,
            sizeof(type_spec_wrapper_bytes),
            &type_spec_wrapper_length,
            diagnostic)) {
        unitlab_free_ied_model_name_list(names, name_count);
        return 0;
    }

    {
        uint8_t false_byte[1U] = { 0x00U };

        if (!server_runtime_encode_ber_element(
                UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
                0,
                0U,
                false_byte,
                sizeof(false_byte),
                response_payload_bytes,
                sizeof(response_payload_bytes),
                &response_payload_length,
                diagnostic)) {
            unitlab_free_ied_model_name_list(names, name_count);
            return 0;
        }
    }
    if (response_payload_length + type_spec_wrapper_length > sizeof(response_payload_bytes)) {
        unitlab_free_ied_model_name_list(names, name_count);
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "GVA response output buffer is too small.");
        return 0;
    }
    memcpy(&response_payload_bytes[response_payload_length], type_spec_wrapper_bytes, type_spec_wrapper_length);
    response_payload_length += type_spec_wrapper_length;
    if (!server_runtime_encode_ber_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            1,
            6U,
            response_payload_bytes,
            response_payload_length,
            service_payload_bytes,
            sizeof(service_payload_bytes),
            &service_payload_length,
            diagnostic)) {
        unitlab_free_ied_model_name_list(names, name_count);
        return 0;
    }
    if (!server_runtime_encode_invoke_id_element(
            invoke_id,
            service_bytes,
            sizeof(service_bytes),
            &invoke_id_length,
            diagnostic)) {
        unitlab_free_ied_model_name_list(names, name_count);
        return 0;
    }
    if (invoke_id_length + service_payload_length > sizeof(service_bytes)) {
        unitlab_free_ied_model_name_list(names, name_count);
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "GVA response output buffer is too small.");
        return 0;
    }
    memcpy(&service_bytes[invoke_id_length], service_payload_bytes, service_payload_length);
    total_length = invoke_id_length + service_payload_length;
    if (total_length > buffer_length) {
        unitlab_free_ied_model_name_list(names, name_count);
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "GVA response output buffer is too small.");
        return 0;
    }
    memcpy(buffer, service_bytes, total_length);
    *encoded_length = total_length;
    server_runtime_store_name_list_summary(server_runtime, invoke_id, "GetVariableAccessAttributes", "components", (const char* const*)names, name_count);
    unitlab_free_ied_model_name_list(names, name_count);
    server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}



static int server_runtime_parse_named_variable_list_reference(
    const char* request_reference,
    char* logical_device_inst,
    size_t logical_device_inst_size,
    char* logical_node_name,
    size_t logical_node_name_size,
    char* list_name,
    size_t list_name_size)
{
    const char* slash = NULL;
    const char* dollar = NULL;
    size_t logical_device_length = 0U;
    size_t logical_node_length = 0U;
    size_t list_name_length = 0U;

    if (logical_device_inst == NULL || logical_device_inst_size == 0U || logical_node_name == NULL || logical_node_name_size == 0U || list_name == NULL || list_name_size == 0U) {
        return 0;
    }
    logical_device_inst[0] = '\0';
    logical_node_name[0] = '\0';
    list_name[0] = '\0';
    if (request_reference == NULL || request_reference[0] == '\0') {
        return 0;
    }

    slash = strchr(request_reference, '/');
    if (slash == NULL) {
        list_name_length = strlen(request_reference);
        if (list_name_length == 0U || list_name_length >= list_name_size) {
            return 0;
        }
        memcpy(list_name, request_reference, list_name_length + 1U);
        return 1;
    }

    logical_device_length = (size_t)(slash - request_reference);
    if (logical_device_length == 0U || logical_device_length >= logical_device_inst_size) {
        return 0;
    }
    memcpy(logical_device_inst, request_reference, logical_device_length);
    logical_device_inst[logical_device_length] = '\0';

    dollar = strchr(slash + 1, '$');
    if (dollar == NULL || dollar == slash + 1) {
        return 0;
    }
    logical_node_length = (size_t)(dollar - (slash + 1));
    if (logical_node_length == 0U || logical_node_length >= logical_node_name_size) {
        return 0;
    }
    memcpy(logical_node_name, slash + 1, logical_node_length);
    logical_node_name[logical_node_length] = '\0';

    list_name_length = strlen(dollar + 1);
    if (list_name_length == 0U || list_name_length >= list_name_size) {
        return 0;
    }
    memcpy(list_name, dollar + 1, list_name_length + 1U);
    return 1;
}

static const UnitLabIedModelDataSet* server_runtime_find_named_variable_list_data_set(
    const UnitLabMmsServerRuntime* server_runtime,
    const char* request_reference,
    char* logical_device_inst,
    size_t logical_device_inst_size,
    char* logical_node_name,
    size_t logical_node_name_size,
    char* list_name,
    size_t list_name_size)
{
    if (server_runtime == NULL || server_runtime->model_plan == NULL) {
        return NULL;
    }
    if (!server_runtime_parse_named_variable_list_reference(
            request_reference,
            logical_device_inst,
            logical_device_inst_size,
            logical_node_name,
            logical_node_name_size,
            list_name,
            list_name_size)) {
        return NULL;
    }

    if (logical_device_inst[0] == '\0') {
        const UnitLabIedModelDataSet* matched_data_set = NULL;

        for (size_t index = 0U; index < server_runtime->model_plan->data_set_count; index++) {
            const UnitLabIedModelDataSet* data_set = &server_runtime->model_plan->data_sets[index];

            if (strcmp(data_set->name, list_name) != 0) {
                continue;
            }
            if (matched_data_set != NULL) {
                return NULL;
            }
            matched_data_set = data_set;
        }
        if (matched_data_set == NULL) {
            return NULL;
        }
        snprintf(logical_device_inst, logical_device_inst_size, "%s", matched_data_set->logical_device_inst);
        snprintf(logical_node_name, logical_node_name_size, "%s", matched_data_set->logical_node_name);
        return matched_data_set;
    }

    return unitlab_find_ied_model_data_set(server_runtime->model_plan, logical_device_inst, logical_node_name, list_name);
}

static int server_runtime_encode_named_variable_list_object_name(
    const char* domain_id,
    const char* item_id,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    uint8_t domain_bytes[256U];
    uint8_t item_bytes[512U];
    uint8_t domainspecific_bytes[1024U];
    uint8_t object_name_bytes[1200U];
    size_t domain_length = 0U;
    size_t item_length = 0U;
    size_t object_name_length = 0U;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (domain_id == NULL || item_id == NULL || buffer == NULL || encoded_length == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Named variable list object name encoding requires domain, item, buffer, and encoded_length.");
        return 0;
    }
    if (!server_runtime_encode_ber_element(
            UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL,
            0,
            26U,
            (const uint8_t*)domain_id,
            strlen(domain_id),
            domain_bytes,
            sizeof(domain_bytes),
            &domain_length,
            diagnostic)) {
        return 0;
    }
    if (!server_runtime_encode_ber_element(
            UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL,
            0,
            26U,
            (const uint8_t*)item_id,
            strlen(item_id),
            item_bytes,
            sizeof(item_bytes),
            &item_length,
            diagnostic)) {
        return 0;
    }
    if (domain_length + item_length > sizeof(domainspecific_bytes)) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Named variable list object name buffer is too small.");
        return 0;
    }
    memcpy(domainspecific_bytes, domain_bytes, domain_length);
    memcpy(&domainspecific_bytes[domain_length], item_bytes, item_length);
    if (!server_runtime_encode_ber_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            1,
            1U,
            domainspecific_bytes,
            domain_length + item_length,
            object_name_bytes,
            sizeof(object_name_bytes),
            &object_name_length,
            diagnostic)) {
        return 0;
    }
    if (object_name_length > buffer_length) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Named variable list object name output buffer is too small.");
        return 0;
    }
    memcpy(buffer, object_name_bytes, object_name_length);
    *encoded_length = object_name_length;
    return 1;
}

static int server_runtime_encode_named_variable_list_member_item(
    const UnitLabIedModelSignal* signal,
    char* domain_id,
    size_t domain_id_size,
    char* item_id,
    size_t item_id_size,
    UnitLabMmsDiagnostic* diagnostic)
{
    const char* entry = NULL;
    const char* slash = NULL;
    size_t domain_length = 0U;
    size_t item_length = 0U;
    int written;

    if (domain_id != NULL && domain_id_size > 0U) {
        domain_id[0] = '\0';
    }
    if (item_id != NULL && item_id_size > 0U) {
        item_id[0] = '\0';
    }
    if (signal == NULL || domain_id == NULL || domain_id_size == 0U || item_id == NULL || item_id_size == 0U) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Named variable list member item encoding requires a signal, output buffers, and output lengths.");
        return 0;
    }

    entry = signal->data_set_entry_variable;
    if (entry != NULL && entry[0] != '\0') {
        slash = strchr(entry, '/');
        if (slash != NULL) {
            domain_length = (size_t)(slash - entry);
            if (domain_length == 0U || domain_length >= domain_id_size) {
                server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Named variable list member domain output buffer is too small.");
                return 0;
            }
            memcpy(domain_id, entry, domain_length);
            domain_id[domain_length] = '\0';
            entry = slash + 1;
        }
    }
    if (domain_id[0] == '\0' && signal->logical_device_inst[0] != '\0') {
        written = snprintf(domain_id, domain_id_size, "%s", signal->logical_device_inst);
        if (written <= 0 || (size_t)written >= domain_id_size) {
            server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Named variable list member domain output buffer is too small.");
            return 0;
        }
    }
    if (entry == NULL || entry[0] == '\0') {
        if (signal->logical_node_name[0] == '\0' || signal->fc[0] == '\0' || signal->object_reference[0] == '\0') {
            server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Named variable list member item encoding requires a dataset entry variable or logical node, functional constraint, and object reference.");
            return 0;
        }

        written = snprintf(item_id, item_id_size, "%s$%s$", signal->logical_node_name, signal->fc);
        if (written <= 0 || (size_t)written >= item_id_size) {
            server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Named variable list member item output buffer is too small.");
            return 0;
        }
        item_length = (size_t)written;
        for (const char* cursor = signal->object_reference; *cursor != '\0'; cursor++) {
            if (item_length + 1U >= item_id_size) {
                server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Named variable list member item output buffer is too small.");
                return 0;
            }
            item_id[item_length++] = (*cursor == '.') ? '$' : *cursor;
        }
        item_id[item_length] = '\0';
        return 1;
    }
    written = snprintf(item_id, item_id_size, "%s", entry);
    if (written <= 0 || (size_t)written >= item_id_size) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Named variable list member item output buffer is too small.");
        return 0;
    }
    return 1;
}

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

static void server_runtime_log_get_named_variable_list_attributes_member_tree(
    size_t member_index,
    const uint8_t* member_bytes,
    size_t member_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsBerElement member;
    UnitLabMmsBerElement variable_spec;
    UnitLabMmsBerElement object_name;
    UnitLabMmsBerElement child;
    size_t consumed_length = 0U;
    size_t child_consumed_length = 0U;
    size_t offset = 0U;
    char domain_id[128U];
    char item_id[256U];

    if (member_bytes == NULL || member_length == 0U) {
        return;
    }

    domain_id[0] = '\0';
    item_id[0] = '\0';
    unitlab_mms_ber_element_init(&member);
    if (!unitlab_mms_ber_read(&member, member_bytes, member_length, &consumed_length, diagnostic)) {
        printf("native-wire-server: gnvla-ber member[%zu] decode-failed\n", member_index);
        fflush(stdout);
        return;
    }
    server_runtime_log_ber_element_line("native-wire-server: gnvla-ber member", &member);

    unitlab_mms_ber_element_init(&variable_spec);
    if (!unitlab_mms_ber_read(&variable_spec, member.value_bytes, member.value_length, &consumed_length, diagnostic)) {
        printf("native-wire-server: gnvla-ber member[%zu] variable-spec decode-failed\n", member_index);
        fflush(stdout);
        return;
    }
    printf(
        "native-wire-server: gnvla-ber member[%zu] VariableSpecification choice tag=%s constructed=%u number=%u length=%zu\n",
        member_index,
        server_runtime_ber_tag_class_label(variable_spec.tag.tag_class),
        (unsigned)variable_spec.tag.constructed,
        (unsigned)variable_spec.tag.tag_number,
        variable_spec.value_length);

    unitlab_mms_ber_element_init(&object_name);
    if (!unitlab_mms_ber_read(&object_name, variable_spec.value_bytes, variable_spec.value_length, &consumed_length, diagnostic)) {
        printf("native-wire-server: gnvla-ber member[%zu] object-name decode-failed\n", member_index);
        fflush(stdout);
        return;
    }
    printf(
        "native-wire-server: gnvla-ber member[%zu] ObjectName choice tag=%s constructed=%u number=%u length=%zu\n",
        member_index,
        server_runtime_ber_tag_class_label(object_name.tag.tag_class),
        (unsigned)object_name.tag.constructed,
        (unsigned)object_name.tag.tag_number,
        object_name.value_length);

    unitlab_mms_ber_element_init(&child);
    if (!unitlab_mms_ber_read(&child, object_name.value_bytes, object_name.value_length, &child_consumed_length, diagnostic)) {
        printf("native-wire-server: gnvla-ber member[%zu] domain-id decode-failed\n", member_index);
        fflush(stdout);
        return;
    }
    if (child.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL && child.tag.tag_number == 26U && !child.tag.constructed) {
        size_t copy_length = child.value_length < sizeof(domain_id) - 1U ? child.value_length : sizeof(domain_id) - 1U;
        memcpy(domain_id, child.value_bytes, copy_length);
        domain_id[copy_length] = '\0';
    }
    printf(
        "native-wire-server: gnvla-ber member[%zu] ObjectName.domainId tag=%s constructed=%u number=%u value=\"%s\"\n",
        member_index,
        server_runtime_ber_tag_class_label(child.tag.tag_class),
        (unsigned)child.tag.constructed,
        (unsigned)child.tag.tag_number,
        domain_id);
    offset += child_consumed_length;

    unitlab_mms_ber_element_init(&child);
    if (offset < object_name.value_length && unitlab_mms_ber_read(&child, &object_name.value_bytes[offset], object_name.value_length - offset, &child_consumed_length, diagnostic)) {
        if (child.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL && child.tag.tag_number == 26U && !child.tag.constructed) {
            size_t copy_length = child.value_length < sizeof(item_id) - 1U ? child.value_length : sizeof(item_id) - 1U;
            memcpy(item_id, child.value_bytes, copy_length);
            item_id[copy_length] = '\0';
        }
        printf(
            "native-wire-server: gnvla-ber member[%zu] ObjectName.itemId tag=%s constructed=%u number=%u value=\"%s\"\n",
            member_index,
            server_runtime_ber_tag_class_label(child.tag.tag_class),
            (unsigned)child.tag.constructed,
            (unsigned)child.tag.tag_number,
            item_id);
    }
    fflush(stdout);
}

static void server_runtime_log_get_named_variable_list_attributes_response_tree(
    uint32_t invoke_id,
    const uint8_t* service_bytes,
    size_t service_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsBerElement invoke_id_element;
    UnitLabMmsBerElement service_element;
    UnitLabMmsBerElement deletable_element;
    UnitLabMmsBerElement list_of_variable_element;
    UnitLabMmsBerElement member;
    size_t consumed_length = 0U;
    size_t response_consumed_length = 0U;
    size_t member_consumed_length = 0U;
    size_t offset = 0U;
    size_t member_index = 0U;

    if (service_bytes == NULL || service_length == 0U) {
        return;
    }
    unitlab_mms_ber_element_init(&invoke_id_element);
    if (!unitlab_mms_ber_read(&invoke_id_element, service_bytes, service_length, &consumed_length, diagnostic)) {
        printf("native-wire-server: gnvla-ber invoke=%u decode-failed-at-invoke\n", (unsigned)invoke_id);
        fflush(stdout);
        return;
    }
    server_runtime_log_ber_element_line("native-wire-server: gnvla-ber invoke-id", &invoke_id_element);

    unitlab_mms_ber_element_init(&service_element);
    if (!unitlab_mms_ber_read(&service_element, &service_bytes[consumed_length], service_length - consumed_length, &response_consumed_length, diagnostic)) {
        printf("native-wire-server: gnvla-ber invoke=%u decode-failed-at-response\n", (unsigned)invoke_id);
        fflush(stdout);
        return;
    }
    server_runtime_log_ber_element_line("native-wire-server: gnvla-ber response", &service_element);

    unitlab_mms_ber_element_init(&deletable_element);
    if (!unitlab_mms_ber_read(&deletable_element, service_element.value_bytes, service_element.value_length, &consumed_length, diagnostic)) {
        printf("native-wire-server: gnvla-ber invoke=%u decode-failed-at-deletable\n", (unsigned)invoke_id);
        fflush(stdout);
        return;
    }
    printf(
        "native-wire-server: gnvla-ber invoke=%u deletable tag=%s constructed=%u number=%u value=%u\n",
        (unsigned)invoke_id,
        server_runtime_ber_tag_class_label(deletable_element.tag.tag_class),
        (unsigned)deletable_element.tag.constructed,
        (unsigned)deletable_element.tag.tag_number,
        deletable_element.value_length > 0U && deletable_element.value_bytes[0] != 0U ? 1U : 0U);

    unitlab_mms_ber_element_init(&list_of_variable_element);
    if (!unitlab_mms_ber_read(&list_of_variable_element, &service_element.value_bytes[consumed_length], service_element.value_length - consumed_length, &response_consumed_length, diagnostic)) {
        printf("native-wire-server: gnvla-ber invoke=%u decode-failed-at-listOfVariable\n", (unsigned)invoke_id);
        fflush(stdout);
        return;
    }
    printf(
        "native-wire-server: gnvla-ber invoke=%u listOfVariable tag=%s constructed=%u number=%u length=%zu\n",
        (unsigned)invoke_id,
        server_runtime_ber_tag_class_label(list_of_variable_element.tag.tag_class),
        (unsigned)list_of_variable_element.tag.constructed,
        (unsigned)list_of_variable_element.tag.tag_number,
        list_of_variable_element.value_length);

    while (offset < list_of_variable_element.value_length) {
        unitlab_mms_ber_element_init(&member);
        if (!unitlab_mms_ber_read(&member, &list_of_variable_element.value_bytes[offset], list_of_variable_element.value_length - offset, &member_consumed_length, diagnostic)) {
            printf("native-wire-server: gnvla-ber invoke=%u member[%zu] decode-failed\n", (unsigned)invoke_id, member_index);
            fflush(stdout);
            return;
        }
        server_runtime_log_get_named_variable_list_attributes_member_tree(
            member_index,
            &list_of_variable_element.value_bytes[offset],
            member_consumed_length,
            diagnostic);
        offset += member_consumed_length;
        member_index++;
    }
}

static void server_runtime_log_read_response_tree(
    uint32_t invoke_id,
    const uint8_t* service_bytes,
    size_t service_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsBerElement invoke_id_element;
    UnitLabMmsBerElement service_element;
    UnitLabMmsBerElement access_result_element;
    UnitLabMmsBerElement data_element;
    char data_tag_hex[32U];
    size_t consumed_length = 0U;
    size_t response_consumed_length = 0U;
    size_t access_result_consumed_length = 0U;
    size_t data_consumed_length = 0U;
    size_t offset = 0U;
    size_t access_result_count = 0U;

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

    while (offset < service_element.value_length) {
        unitlab_mms_ber_element_init(&access_result_element);
        if (!unitlab_mms_ber_read(&access_result_element, &service_element.value_bytes[offset], service_element.value_length - offset, &access_result_consumed_length, diagnostic)) {
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
        if (unitlab_mms_ber_read(&data_element, access_result_element.value_bytes, access_result_element.value_length, &data_consumed_length, diagnostic)) {
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
                    access_result_element.tag.tag_number == 0U ? "success" : "failure",
                    data_tag_hex[0] != '\0' ? data_tag_hex : "<invalid>",
                    data_element.value_length,
                    printable_value);
            } else {
                printf(
                    "native-wire-server: read-ber invoke=%u accessResult[%zu] status=%s data-tag=%s value-length=%zu\n",
                    (unsigned)invoke_id,
                    access_result_count,
                    access_result_element.tag.tag_number == 0U ? "success" : "failure",
                    data_tag_hex[0] != '\0' ? data_tag_hex : "<invalid>",
                    data_element.value_length);
            }
        } else {
            printf(
                "native-wire-server: read-ber invoke=%u accessResult[%zu] status=%s data-decode-failed\n",
                (unsigned)invoke_id,
                access_result_count,
                access_result_element.tag.tag_number == 0U ? "success" : "failure");
        }
        fflush(stdout);
        offset += access_result_consumed_length;
        access_result_count++;
    }
    printf("native-wire-server: read-ber invoke=%u accessResult-count=%zu\n", (unsigned)invoke_id, access_result_count);
    fflush(stdout);
}
static int server_runtime_encode_named_variable_list_member(

    const char* domain_id,
    const char* item_id,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    uint8_t object_name_bytes[1300U];
    uint8_t variable_spec_bytes[1600U];
    uint8_t member_bytes[1800U];
    size_t object_name_length = 0U;
    size_t variable_spec_length = 0U;
    size_t member_length = 0U;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (domain_id == NULL || item_id == NULL || buffer == NULL || encoded_length == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Named variable list member encoding requires domain, item, buffer, and encoded_length.");
        return 0;
    }
    if (!server_runtime_encode_named_variable_list_object_name(
            domain_id,
            item_id,
            object_name_bytes,
            sizeof(object_name_bytes),
            &object_name_length,
            diagnostic)) {
        return 0;
    }
    if (!server_runtime_encode_ber_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            1,
            0U,
            object_name_bytes,
            object_name_length,
            variable_spec_bytes,
            sizeof(variable_spec_bytes),
            &variable_spec_length,
            diagnostic)) {
        return 0;
    }
    if (!server_runtime_encode_ber_element(
            UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL,
            1,
            16U,
            variable_spec_bytes,
            variable_spec_length,
            member_bytes,
            sizeof(member_bytes),
            &member_length,
            diagnostic)) {
        return 0;
    }
    if (member_length > buffer_length) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Named variable list member output buffer is too small.");
        return 0;
    }
    memcpy(buffer, member_bytes, member_length);
    *encoded_length = member_length;
    return 1;
}

static int server_runtime_build_get_named_variable_list_attributes_response_service(
    UnitLabMmsServerRuntime* server_runtime,
    uint32_t invoke_id,
    const char* object_reference,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    char logical_device_inst[128U];
    char logical_node_name[128U];
    char list_name[128U];
    const UnitLabIedModelDataSet* data_set = NULL;
    uint8_t member_bytes[8192U];
    uint8_t list_of_variable_bytes[9000U];
    uint8_t response_payload_bytes[9500U];
    uint8_t service_payload_bytes[9600U];
    uint8_t service_bytes[9700U];
    size_t member_bytes_length = 0U;
    size_t list_of_variable_length = 0U;
    size_t response_payload_length = 0U;
    size_t service_payload_length = 0U;
    size_t invoke_id_length = 0U;
    size_t total_length = 0U;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (server_runtime == NULL || buffer == NULL || encoded_length == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "GetNamedVariableListAttributes response buffer and encoded_length are required.");
        return 0;
    }
    if (object_reference == NULL || object_reference[0] == '\0') {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "GetNamedVariableListAttributes response requires a list reference.");
        return 0;
    }
    data_set = server_runtime_find_named_variable_list_data_set(
        server_runtime,
        object_reference,
        logical_device_inst,
        sizeof(logical_device_inst),
        logical_node_name,
        sizeof(logical_node_name),
        list_name,
        sizeof(list_name));
    if (data_set == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "GetNamedVariableListAttributes list lookup failed.");
        return 0;
    }

    printf(
        "native-wire-server: confirmed-response invoke=%u service=GetNamedVariableListAttributes list=%s domain=%s node=%s members=%zu deletable=false\n",
        (unsigned)invoke_id,
        list_name,
        logical_device_inst,
        logical_node_name,
        data_set->member_count);
    fflush(stdout);

    for (size_t member_index = 0U; member_index < data_set->member_count; member_index++) {
        const UnitLabIedModelSignal* signal = &server_runtime->model_plan->signals[data_set->first_signal_index + member_index];
        char member_item[256U];
        size_t member_length = 0U;

        if (!server_runtime_encode_named_variable_list_member_item(
                signal,
                logical_device_inst,
                sizeof(logical_device_inst),
                member_item,
                sizeof(member_item),
                diagnostic)) {
            return 0;
        }
        if (!server_runtime_encode_named_variable_list_member(
                logical_device_inst,
                member_item,
                &member_bytes[member_bytes_length],
                sizeof(member_bytes) - member_bytes_length,
                &member_length,
                diagnostic)) {
            return 0;
        }
        member_bytes_length += member_length;
        printf("native-wire-server: nvl-attribute-member[%zu]=%s/%s\n", member_index, logical_device_inst, member_item);
    }

    {
        uint8_t false_byte[1U] = { 0x00U };
        size_t mms_deletable_length = 0U;

        if (!server_runtime_encode_ber_element(
                UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
                0,
                0U,
                false_byte,
                sizeof(false_byte),
                response_payload_bytes,
                sizeof(response_payload_bytes),
                &mms_deletable_length,
                diagnostic)) {
            return 0;
        }
        response_payload_length = mms_deletable_length;
    }
    if (!server_runtime_encode_ber_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            1,
            1U,
            member_bytes,
            member_bytes_length,
            list_of_variable_bytes,
            sizeof(list_of_variable_bytes),
            &list_of_variable_length,
            diagnostic)) {
        return 0;
    }
    if (response_payload_length + list_of_variable_length > sizeof(response_payload_bytes)) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "GetNamedVariableListAttributes response output buffer is too small.");
        return 0;
    }
    memcpy(&response_payload_bytes[response_payload_length], list_of_variable_bytes, list_of_variable_length);
    response_payload_length += list_of_variable_length;
    if (!server_runtime_encode_ber_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            1,
            12U,
            response_payload_bytes,
            response_payload_length,
            service_payload_bytes,
            sizeof(service_payload_bytes),
            &service_payload_length,
            diagnostic)) {
        return 0;
    }
    if (!server_runtime_encode_invoke_id_element(
            invoke_id,
            service_bytes,
            sizeof(service_bytes),
            &invoke_id_length,
            diagnostic)) {
        return 0;
    }
    if (invoke_id_length + service_payload_length > sizeof(service_bytes)) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "GetNamedVariableListAttributes response output buffer is too small.");
        return 0;
    }
    memcpy(&service_bytes[invoke_id_length], service_payload_bytes, service_payload_length);
    total_length = invoke_id_length + service_payload_length;
    if (total_length > buffer_length) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "GetNamedVariableListAttributes response output buffer is too small.");
        return 0;
    }
    memcpy(buffer, service_bytes, total_length);
    *encoded_length = total_length;
    server_runtime_log_get_named_variable_list_attributes_response_tree(invoke_id, service_bytes, total_length, diagnostic);
    {
        char summary[512U];
        size_t offset = 0U;

        offset = (size_t)snprintf(summary, sizeof(summary), "variables=%zu list=[", data_set->member_count);
        for (size_t member_index = 0U; member_index < data_set->member_count && offset < sizeof(summary); member_index++) {
            const UnitLabIedModelSignal* signal = &server_runtime->model_plan->signals[data_set->first_signal_index + member_index];
            int written = snprintf(&summary[offset], sizeof(summary) - offset, "%s%s", member_index > 0U ? "," : "", signal->data_set_entry_variable);
            if (written < 0) {
                summary[0] = '\0';
                break;
            }
            if ((size_t)written >= sizeof(summary) - offset) {
                offset = sizeof(summary) - 1U;
                break;
            }
            offset += (size_t)written;
        }
        if (offset < sizeof(summary) - 1U) {
            snprintf(&summary[offset], sizeof(summary) - offset, "]");
        }
        server_runtime_store_outgoing_context(server_runtime, invoke_id, "GetNamedVariableListAttributes", summary);
    }
    server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}

int unitlab_mms_server_runtime_apply_model_plan(UnitLabMmsServerRuntime* server_runtime, const UnitLabIedModelPlan* plan)
{
    if (server_runtime == NULL) {
        return 0;
    }
    server_runtime->model_plan = plan;
    unitlab_mms_initiate_response_profile_apply_model_plan(&server_runtime->initiate_response_profile, plan);
    server_runtime->read_response_value[0] = '\0';
    server_runtime->read_response_value_length = 0U;
    server_runtime->has_read_response_value = 0;
    server_runtime->last_incoming_invoke_id = 0U;
    server_runtime->last_incoming_service[0] = '\0';
    server_runtime->last_outgoing_invoke_id = 0U;
    server_runtime->last_outgoing_service[0] = '\0';
    server_runtime->last_outgoing_summary[0] = '\0';
    unitlab_mms_server_runtime_capture_snapshot(server_runtime);
    return 1;
}

static int server_runtime_build_write_response_service(
    uint32_t invoke_id,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    uint8_t member_bytes[16U];
    uint8_t response_sequence_bytes[32U];
    uint8_t response_payload_bytes[48U];
    uint8_t service_bytes[64U];
    uint8_t invoke_id_element_bytes[16U];
    size_t member_length = 0U;
    size_t response_sequence_length = 0U;
    size_t response_payload_length = 0U;
    size_t invoke_id_length = 0U;
    size_t total_length = 0U;
    UnitLabMmsBerElement member_element;
    UnitLabMmsBerElement response_sequence_element;
    UnitLabMmsBerElement response_payload_element;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (buffer == NULL || encoded_length == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Write response service buffer and encoded_length are required.");
        return 0;
    }

    unitlab_mms_ber_element_init(&member_element);
    member_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    member_element.tag.constructed = 0;
    member_element.tag.tag_number = 1U;
    member_element.value_bytes = NULL;
    member_element.value_length = 0U;
    if (!server_runtime_encode_ber_element(
            member_element.tag.tag_class,
            member_element.tag.constructed,
            member_element.tag.tag_number,
            member_element.value_bytes,
            member_element.value_length,
            member_bytes,
            sizeof(member_bytes),
            &member_length,
            diagnostic)) {
        return 0;
    }

    unitlab_mms_ber_element_init(&response_sequence_element);
    response_sequence_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL;
    response_sequence_element.tag.constructed = 1;
    response_sequence_element.tag.tag_number = 16U;
    response_sequence_element.value_bytes = member_bytes;
    response_sequence_element.value_length = member_length;
    if (!server_runtime_encode_ber_element(
            response_sequence_element.tag.tag_class,
            response_sequence_element.tag.constructed,
            response_sequence_element.tag.tag_number,
            response_sequence_element.value_bytes,
            response_sequence_element.value_length,
            response_sequence_bytes,
            sizeof(response_sequence_bytes),
            &response_sequence_length,
            diagnostic)) {
        return 0;
    }

    unitlab_mms_ber_element_init(&response_payload_element);
    response_payload_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    response_payload_element.tag.constructed = 1;
    response_payload_element.tag.tag_number = 5U;
    response_payload_element.value_bytes = response_sequence_bytes;
    response_payload_element.value_length = response_sequence_length;
    if (!server_runtime_encode_ber_element(
            response_payload_element.tag.tag_class,
            response_payload_element.tag.constructed,
            response_payload_element.tag.tag_number,
            response_payload_element.value_bytes,
            response_payload_element.value_length,
            response_payload_bytes,
            sizeof(response_payload_bytes),
            &response_payload_length,
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

    if (invoke_id_length + response_payload_length > sizeof(service_bytes)) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Write response service buffer is too small.");
        return 0;
    }
    memcpy(service_bytes, invoke_id_element_bytes, invoke_id_length);
    memcpy(service_bytes + invoke_id_length, response_payload_bytes, response_payload_length);
    total_length = invoke_id_length + response_payload_length;
    if (total_length > buffer_length) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Write response output buffer is too small.");
        return 0;
    }
    memcpy(buffer, service_bytes, total_length);
    *encoded_length = total_length;
    server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}

static int server_runtime_object_reference_has_suffix(const char* object_reference, const char* suffix)
{
    size_t object_length = 0U;
    size_t suffix_length = 0U;

    if (object_reference == NULL || suffix == NULL) {
        return 0;
    }
    object_length = strlen(object_reference);
    suffix_length = strlen(suffix);
    if (suffix_length == 0U || object_length < suffix_length) {
        return 0;
    }
    return strcmp(object_reference + (object_length - suffix_length), suffix) == 0;
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

    signal = server_runtime_find_signal_by_object_reference(server_runtime, object_reference);
    if (signal != NULL) {
        if (server_runtime_encode_mms_data_value(signal, buffer, buffer_length, encoded_length, diagnostic)) {
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
    for (size_t rcb_field_index = 0U; rcb_field_index < sizeof(lln0_br_rcb_children) / sizeof(lln0_br_rcb_children[0]); rcb_field_index++) {
        char brcb_suffix[128U];
        char legacy_suffix[160U];

        snprintf(brcb_suffix, sizeof(brcb_suffix), ".BR.brcbEvents.%s", lln0_br_rcb_children[rcb_field_index]);
        snprintf(legacy_suffix, sizeof(legacy_suffix), ".BR.LLN0_Events_BuffRep01.%s", lln0_br_rcb_children[rcb_field_index]);
        if (server_runtime_object_reference_has_suffix(object_reference, brcb_suffix)
            || server_runtime_object_reference_has_suffix(object_reference, legacy_suffix)) {
            if (!server_runtime_encode_report_control_block_field_value(
                    lln0_br_rcb_children[rcb_field_index],
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
        synthetic_signal.initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_STRING;
        snprintf(synthetic_signal.initial_value, sizeof(synthetic_signal.initial_value), "%s", "LD0/LLN0$dsEvents");
        if (!server_runtime_encode_mms_data_value(&synthetic_signal, buffer, buffer_length, encoded_length, diagnostic)) {
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
        integer_value = 0;
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
        uint8_t boolean_value = 0x00U;

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
    if (server_runtime_object_reference_has_suffix_any(object_reference, (const char* const[]){ ".BR.brcbEvents", ".BR.LLN0_Events_BuffRep01" }, 2U)) {
        if (!server_runtime_encode_report_control_block_value(buffer, buffer_length, encoded_length, diagnostic)) {
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

static int server_runtime_build_read_response_service(
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
    uint8_t read_response_body_bytes[288U];
    uint8_t service_bytes[320U];
    uint8_t invoke_id_element_bytes[16U];
    char domain_id[128U];
    char item_id[128U];
    size_t value_length = 0U;
    size_t access_result_value_length = 0U;
    size_t list_of_access_result_length = 0U;
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
        if (!server_runtime_encode_ber_element(
                UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
                1,
                value_supported ? 0U : 1U,
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
            4U,
            list_of_access_result_bytes,
            list_of_access_result_length,
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

static int server_runtime_decode_transport_to_wire_pdu(
    const uint8_t* buffer,
    size_t buffer_length,
    size_t* consumed_length,
    UnitLabMmsPdu* wire_pdu,
    UnitLabMmsOperationResult* operation_result)
{
    UnitLabMmsTransportFrame transport_frame;
    UnitLabMmsSessionSpdu session_spdu;
    UnitLabMmsPresentationApdu presentation_apdu;
    const uint8_t* presentation_bytes = NULL;
    const uint8_t* decode_bytes = NULL;
    size_t presentation_length = 0U;
    size_t decode_length = 0U;
    size_t transport_consumed_length = 0U;
    size_t session_consumed_length = 0U;
    size_t presentation_consumed_length = 0U;
    size_t pdu_consumed_length = 0U;

    if (!unitlab_mms_transport_frame_decode(&transport_frame, buffer, buffer_length, &transport_consumed_length, &operation_result->diagnostic)) {
        return 0;
    }
    if (transport_frame.cotp.user_data_length == 0U || transport_frame.cotp.user_data == NULL) {
        operation_result->diagnostic.code = UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR;
        snprintf(operation_result->diagnostic.message, sizeof(operation_result->diagnostic.message), "%s", "Transport frame is missing session bytes.");
        return 0;
    }
    unitlab_mms_session_spdu_init(&session_spdu);
    if (!unitlab_mms_session_spdu_decode(&session_spdu, transport_frame.cotp.user_data, transport_frame.cotp.user_data_length, &session_consumed_length, &operation_result->diagnostic)) {
        return 0;
    }
    if (session_consumed_length != transport_frame.cotp.user_data_length) {
        operation_result->diagnostic.code = UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR;
        snprintf(operation_result->diagnostic.message, sizeof(operation_result->diagnostic.message), "%s", "Incoming bytes contain trailing session bytes.");
        return 0;
    }
    if (session_spdu.raw_parameter_length == 0U || session_spdu.raw_parameter_bytes == NULL) {
        operation_result->diagnostic.code = UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR;
        snprintf(operation_result->diagnostic.message, sizeof(operation_result->diagnostic.message), "%s", "Session SPDU is missing presentation bytes.");
        return 0;
    }
    presentation_bytes = session_spdu.raw_parameter_bytes;
    presentation_length = session_spdu.raw_parameter_length;
    decode_bytes = presentation_bytes;
    decode_length = presentation_length;
    unitlab_mms_presentation_apdu_init(&presentation_apdu);
    if (unitlab_mms_presentation_decode(&presentation_apdu, presentation_bytes, presentation_length, &presentation_consumed_length, &operation_result->diagnostic)) {
        if (presentation_consumed_length != presentation_length) {
            operation_result->diagnostic.code = UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR;
            snprintf(operation_result->diagnostic.message, sizeof(operation_result->diagnostic.message), "%s", "Incoming bytes contain trailing presentation bytes.");
            return 0;
        }
        if (presentation_apdu.payload_length == 0U || presentation_apdu.payload_bytes == NULL) {
            operation_result->diagnostic.code = UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR;
            snprintf(operation_result->diagnostic.message, sizeof(operation_result->diagnostic.message), "%s", "Presentation User-data is missing MMS bytes.");
            return 0;
        }
        decode_bytes = presentation_apdu.payload_bytes;
        decode_length = presentation_apdu.payload_length;
    }
    unitlab_mms_pdu_init(wire_pdu);
    if (unitlab_mms_pdu_decode(wire_pdu, decode_bytes, decode_length, &pdu_consumed_length, &operation_result->diagnostic)) {
        if (pdu_consumed_length != decode_length) {
            operation_result->diagnostic.code = UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR;
            snprintf(operation_result->diagnostic.message, sizeof(operation_result->diagnostic.message), "%s", "Incoming bytes contain trailing MMS bytes.");
            return 0;
        }
        *consumed_length = transport_consumed_length;
        return 1;
    }

    {
        UnitLabMmsAcseApdu acse_apdu;
        size_t acse_consumed_length = 0U;

        unitlab_mms_acse_apdu_init(&acse_apdu);
        if (!unitlab_mms_acse_decode(&acse_apdu, decode_bytes, decode_length, &acse_consumed_length, &operation_result->diagnostic)) {
            return 0;
        }
        if (acse_consumed_length != decode_length) {
            operation_result->diagnostic.code = UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR;
            snprintf(operation_result->diagnostic.message, sizeof(operation_result->diagnostic.message), "%s", "Incoming bytes contain trailing ACSE bytes.");
            return 0;
        }
        if (acse_apdu.kind != UNITLAB_MMS_ACSE_APDU_AARQ) {
            operation_result->diagnostic.code = UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED;
            snprintf(operation_result->diagnostic.message, sizeof(operation_result->diagnostic.message), "%s", "Association request bytes must carry an ACSE AARQ or MMS initiate request.");
            return 0;
        }
        unitlab_mms_pdu_init(wire_pdu);
        wire_pdu->kind = UNITLAB_MMS_PDU_INITIATE_REQUEST;
        *consumed_length = transport_consumed_length;
        return 1;
    }
}

void unitlab_mms_server_runtime_init(UnitLabMmsServerRuntime* server_runtime)
{
    if (server_runtime == NULL) {
        return;
    }
    memset(&server_runtime->config, 0, sizeof(server_runtime->config));
    server_runtime->state = UNITLAB_MMS_SERVER_RUNTIME_IDLE;
    server_runtime->model_plan = NULL;
    unitlab_mms_session_init(&server_runtime->session);
    unitlab_mms_pending_request_init(&server_runtime->pending_request);
    unitlab_mms_initiate_response_profile_init(&server_runtime->initiate_response_profile);
    server_runtime->read_response_value[0] = '\0';
    server_runtime->read_response_value_length = 0U;
    server_runtime->has_read_response_value = 0;
    unitlab_iec61850_report_control_init(&server_runtime->report_control);
    unitlab_mms_transport_exchange_init(&server_runtime->transport);
    unitlab_mms_operation_result_init(&server_runtime->last_result);
    unitlab_mms_runtime_snapshot_init(&server_runtime->snapshot);
    unitlab_mms_pdu_init(&server_runtime->last_wire_pdu);
    unitlab_mms_runtime_snapshot_capture(
        &server_runtime->snapshot,
        &server_runtime->session,
        &server_runtime->report_control,
        &server_runtime->transport,
        &server_runtime->last_result);
}

int unitlab_mms_server_runtime_prepare(UnitLabMmsServerRuntime* server_runtime, const UnitLabIedServerConfig* config, UnitLabMmsDiagnostic* diagnostic)
{
    if (server_runtime == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Server runtime is required.");
        return 0;
    }
    if (server_runtime->state != UNITLAB_MMS_SERVER_RUNTIME_IDLE && server_runtime->state != UNITLAB_MMS_SERVER_RUNTIME_STOPPED) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BAD_STATE, "Server runtime must be idle or stopped before prepare.");
        return 0;
    }
    if (!server_runtime_validate_config(config, diagnostic)) {
        server_runtime->state = UNITLAB_MMS_SERVER_RUNTIME_FAILED;
        return 0;
    }
    server_runtime->config = *config;
    server_runtime->state = UNITLAB_MMS_SERVER_RUNTIME_PREPARED;
    server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    unitlab_mms_server_runtime_capture_snapshot(server_runtime);
    return 1;
}

int unitlab_mms_server_runtime_start(UnitLabMmsServerRuntime* server_runtime, UnitLabMmsDiagnostic* diagnostic)
{
    if (server_runtime == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Server runtime is required.");
        return 0;
    }
    if (server_runtime->state != UNITLAB_MMS_SERVER_RUNTIME_PREPARED) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BAD_STATE, "Server runtime must be prepared before start.");
        return 0;
    }
    server_runtime->state = UNITLAB_MMS_SERVER_RUNTIME_RUNNING;
    server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    unitlab_mms_server_runtime_capture_snapshot(server_runtime);
    return 1;
}

int unitlab_mms_server_runtime_stop(UnitLabMmsServerRuntime* server_runtime, UnitLabMmsDiagnostic* diagnostic)
{
    if (server_runtime == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Server runtime is required.");
        return 0;
    }
    if (server_runtime->state == UNITLAB_MMS_SERVER_RUNTIME_IDLE) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BAD_STATE, "Server runtime cannot stop before prepare.");
        return 0;
    }
    if (server_runtime->state == UNITLAB_MMS_SERVER_RUNTIME_STOPPED) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
        return 1;
    }
    if (server_runtime->state != UNITLAB_MMS_SERVER_RUNTIME_PREPARED && server_runtime->state != UNITLAB_MMS_SERVER_RUNTIME_RUNNING) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BAD_STATE, "Server runtime cannot stop from the current state.");
        return 0;
    }
    server_runtime->state = UNITLAB_MMS_SERVER_RUNTIME_STOPPED;
    server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    unitlab_mms_server_runtime_capture_snapshot(server_runtime);
    return 1;
}

static int server_runtime_prepare_confirmed_response_pdu(
    const UnitLabMmsServerRuntime* server_runtime,
    const uint8_t* service_bytes,
    size_t service_length,
    UnitLabMmsPdu* response_pdu,
    UnitLabMmsDiagnostic* diagnostic)
{
    if (server_runtime == NULL || response_pdu == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Server runtime and response PDU are required.");
        return 0;
    }
    if (server_runtime->pending_request.state != UNITLAB_MMS_PENDING_REQUEST_ACTIVE) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BAD_STATE, "Pending request must be active before building a response.");
        return 0;
    }
    if (server_runtime->pending_request.kind != UNITLAB_MMS_REQUEST_READ && server_runtime->pending_request.kind != UNITLAB_MMS_REQUEST_WRITE && server_runtime->pending_request.kind != UNITLAB_MMS_REQUEST_GET_NAME_LIST && server_runtime->pending_request.kind != UNITLAB_MMS_REQUEST_GET_VARIABLE_ACCESS_ATTRIBUTES && server_runtime->pending_request.kind != UNITLAB_MMS_REQUEST_GET_NAMED_VARIABLE_LIST_ATTRIBUTES) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "Only first-slice READ/WRITE/GetNameList/GetVariableAccessAttributes/GetNamedVariableListAttributes responses are supported.");
        return 0;
    }
    unitlab_mms_pdu_init(response_pdu);
    response_pdu->kind = UNITLAB_MMS_PDU_CONFIRMED_RESPONSE;
    response_pdu->has_invoke_id = 1;
    response_pdu->invoke_id = server_runtime->pending_request.invoke_id;
    response_pdu->has_service = 1;
    if (server_runtime->pending_request.kind == UNITLAB_MMS_REQUEST_READ) {
        response_pdu->service_kind = UNITLAB_MMS_SERVICE_READ;
    } else if (server_runtime->pending_request.kind == UNITLAB_MMS_REQUEST_WRITE) {
        response_pdu->service_kind = UNITLAB_MMS_SERVICE_WRITE;
    } else if (server_runtime->pending_request.kind == UNITLAB_MMS_REQUEST_GET_NAME_LIST) {
        response_pdu->service_kind = UNITLAB_MMS_SERVICE_GET_NAME_LIST;
    } else if (server_runtime->pending_request.kind == UNITLAB_MMS_REQUEST_GET_VARIABLE_ACCESS_ATTRIBUTES) {
        response_pdu->service_kind = UNITLAB_MMS_SERVICE_GET_VARIABLE_ACCESS_ATTRIBUTES;
    } else if (server_runtime->pending_request.kind == UNITLAB_MMS_REQUEST_GET_NAMED_VARIABLE_LIST_ATTRIBUTES) {
        response_pdu->service_kind = UNITLAB_MMS_SERVICE_GET_NAMED_VARIABLE_LIST_ATTRIBUTES;
    }
    response_pdu->pdu_bytes = service_bytes;
    response_pdu->pdu_length = service_length;
    return 1;
}

static int server_runtime_require_running(UnitLabMmsServerRuntime* server_runtime, UnitLabMmsDiagnostic* diagnostic)
{
    if (server_runtime->state != UNITLAB_MMS_SERVER_RUNTIME_RUNNING) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BAD_STATE, "Server runtime must be running for report-control operations.");
        return 0;
    }
    return 1;
}

int unitlab_mms_server_runtime_reserve_report_control(UnitLabMmsServerRuntime* server_runtime, UnitLabMmsDiagnostic* diagnostic)
{
    if (server_runtime == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Server runtime is required.");
        return 0;
    }
    if (!server_runtime_require_running(server_runtime, diagnostic)) {
        return 0;
    }
    if (!unitlab_iec61850_report_control_reserve(&server_runtime->report_control, diagnostic)) {
        return 0;
    }
    server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    unitlab_mms_server_runtime_capture_snapshot(server_runtime);
    return 1;
}

int unitlab_mms_server_runtime_enable_report_control(UnitLabMmsServerRuntime* server_runtime, UnitLabMmsDiagnostic* diagnostic)
{
    if (server_runtime == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Server runtime is required.");
        return 0;
    }
    if (!server_runtime_require_running(server_runtime, diagnostic)) {
        return 0;
    }
    if (!unitlab_iec61850_report_control_enable(&server_runtime->report_control, diagnostic)) {
        return 0;
    }
    server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    unitlab_mms_server_runtime_capture_snapshot(server_runtime);
    return 1;
}

int unitlab_mms_server_runtime_request_general_interrogation(UnitLabMmsServerRuntime* server_runtime, UnitLabMmsDiagnostic* diagnostic)
{
    if (server_runtime == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Server runtime is required.");
        return 0;
    }
    if (!server_runtime_require_running(server_runtime, diagnostic)) {
        return 0;
    }
    if (!unitlab_iec61850_report_control_request_gi(&server_runtime->report_control, diagnostic)) {
        return 0;
    }
    server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    unitlab_mms_server_runtime_capture_snapshot(server_runtime);
    return 1;
}

int unitlab_mms_server_runtime_disable_report_control(UnitLabMmsServerRuntime* server_runtime, UnitLabMmsDiagnostic* diagnostic)
{
    if (server_runtime == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Server runtime is required.");
        return 0;
    }
    if (!server_runtime_require_running(server_runtime, diagnostic)) {
        return 0;
    }
    if (!unitlab_iec61850_report_control_disable(&server_runtime->report_control, diagnostic)) {
        return 0;
    }
    server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    unitlab_mms_server_runtime_capture_snapshot(server_runtime);
    return 1;
}

int unitlab_mms_server_runtime_release_report_control(UnitLabMmsServerRuntime* server_runtime, UnitLabMmsDiagnostic* diagnostic)
{
    if (server_runtime == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Server runtime is required.");
        return 0;
    }
    if (!server_runtime_require_running(server_runtime, diagnostic)) {
        return 0;
    }
    if (!unitlab_iec61850_report_control_release(&server_runtime->report_control, diagnostic)) {
        return 0;
    }
    server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    unitlab_mms_server_runtime_capture_snapshot(server_runtime);
    return 1;
}

int unitlab_mms_server_runtime_build_confirmed_error_bytes(UnitLabMmsServerRuntime* server_runtime, uint32_t invoke_id, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsPdu response_pdu;
    uint8_t synthesized_service_bytes[32U];
    uint8_t service_error_value[5U] = { 0xA0U, 0x03U, 0x84U, 0x01U, 0x00U };
    size_t invoke_id_length = 0U;
    size_t service_error_length = 0U;
    size_t total_length = 0U;
    size_t response_length = 0U;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (server_runtime == NULL || buffer == NULL || encoded_length == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Server runtime, buffer, and encoded_length are required.");
        return 0;
    }
    if (server_runtime->state != UNITLAB_MMS_SERVER_RUNTIME_RUNNING) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BAD_STATE, "Server runtime must be running before building confirmed error bytes.");
        return 0;
    }

    if (!server_runtime_encode_confirmed_error_invoke_id_element(
            invoke_id,
            synthesized_service_bytes,
            sizeof(synthesized_service_bytes),
            &invoke_id_length,
            diagnostic)) {
        return 0;
    }
    if (!server_runtime_encode_ber_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            1,
            2U,
            service_error_value,
            sizeof(service_error_value),
            &synthesized_service_bytes[invoke_id_length],
            sizeof(synthesized_service_bytes) - invoke_id_length,
            &service_error_length,
            diagnostic)) {
        return 0;
    }
    total_length = invoke_id_length + service_error_length;
    if (total_length > sizeof(synthesized_service_bytes)) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Confirmed error service buffer is too small.");
        return 0;
    }

    unitlab_mms_pdu_init(&response_pdu);
    response_pdu.kind = UNITLAB_MMS_PDU_CONFIRMED_ERROR;
    response_pdu.has_invoke_id = 1;
    response_pdu.invoke_id = invoke_id;
    response_pdu.has_service = 1;
    response_pdu.service_kind = UNITLAB_MMS_SERVICE_RAW;
    response_pdu.pdu_bytes = synthesized_service_bytes;
    response_pdu.pdu_length = total_length;
    if (!unitlab_mms_build_wire_frame_from_pdu(
            &response_pdu,
            server_runtime->wire_scratch,
            sizeof(server_runtime->wire_scratch),
            buffer,
            buffer_length,
            &response_length,
            diagnostic)) {
        return 0;
    }
    if (!unitlab_mms_transport_exchange_bind_response(&server_runtime->transport, buffer, buffer_length, diagnostic)) {
        return 0;
    }
    if (!unitlab_mms_transport_exchange_set_response_length(&server_runtime->transport, response_length, diagnostic)) {
        return 0;
    }
    *encoded_length = response_length;
    server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    unitlab_mms_operation_result_from_trace(
        &server_runtime->last_result,
        1,
        diagnostic,
        &server_runtime->transport.event_log,
        &server_runtime->transport.last_event);
    unitlab_mms_server_runtime_capture_snapshot(server_runtime);
    return 1;
}

int unitlab_mms_server_runtime_build_confirmed_response_bytes(UnitLabMmsServerRuntime* server_runtime, const uint8_t* service_bytes, size_t service_length, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsPdu response_pdu;
    uint8_t synthesized_service_bytes[2048U];
    size_t synthesized_service_length = 0U;
    size_t response_length = 0U;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (server_runtime == NULL || buffer == NULL || encoded_length == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Server runtime, buffer, and encoded_length are required.");
        return 0;
    }
    if (server_runtime->state != UNITLAB_MMS_SERVER_RUNTIME_RUNNING) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BAD_STATE, "Server runtime must be running before building response bytes.");
        return 0;
    }
    if (service_length == 0U) {
        if (server_runtime->pending_request.kind == UNITLAB_MMS_REQUEST_READ) {
            if (!server_runtime_build_read_response_service(
                    server_runtime,
                    server_runtime->pending_request.invoke_id,
                    synthesized_service_bytes,
                    sizeof(synthesized_service_bytes),
                    &synthesized_service_length,
                    diagnostic)) {
                return 0;
            }
            service_bytes = synthesized_service_bytes;
            service_length = synthesized_service_length;
        } else if (server_runtime->pending_request.kind == UNITLAB_MMS_REQUEST_GET_NAME_LIST) {
            if (!server_runtime_build_get_name_list_response_service(
                    server_runtime,
                    server_runtime->pending_request.invoke_id,
                    synthesized_service_bytes,
                    sizeof(synthesized_service_bytes),
                    &synthesized_service_length,
                    diagnostic)) {
                return 0;
            }
            service_bytes = synthesized_service_bytes;
            service_length = synthesized_service_length;
        } else if (server_runtime->pending_request.kind == UNITLAB_MMS_REQUEST_GET_VARIABLE_ACCESS_ATTRIBUTES) {
            if (!server_runtime_build_get_variable_access_attributes_response_service(
                    server_runtime,
                    server_runtime->pending_request.invoke_id,
                    server_runtime->pending_request.object_reference,
                    synthesized_service_bytes,
                    sizeof(synthesized_service_bytes),
                    &synthesized_service_length,
                    diagnostic)) {
                return 0;
            }
            service_bytes = synthesized_service_bytes;
            service_length = synthesized_service_length;
        } else if (server_runtime->pending_request.kind == UNITLAB_MMS_REQUEST_GET_NAMED_VARIABLE_LIST_ATTRIBUTES) {
            if (!server_runtime_build_get_named_variable_list_attributes_response_service(
                    server_runtime,
                    server_runtime->pending_request.invoke_id,
                    server_runtime->pending_request.object_reference,
                    synthesized_service_bytes,
                    sizeof(synthesized_service_bytes),
                    &synthesized_service_length,
                    diagnostic)) {
                return 0;
            }
            service_bytes = synthesized_service_bytes;
            service_length = synthesized_service_length;
        } else if (server_runtime->pending_request.kind == UNITLAB_MMS_REQUEST_WRITE) {
            if (!server_runtime_build_write_response_service(
                    server_runtime->pending_request.invoke_id,
                    synthesized_service_bytes,
                    sizeof(synthesized_service_bytes),
                    &synthesized_service_length,
                    diagnostic)) {
                return 0;
            }
            service_bytes = synthesized_service_bytes;
            service_length = synthesized_service_length;
        } else {
            uint8_t invoke_id_bytes[5U];
            size_t invoke_id_length_bytes = 0U;
            uint32_t value = server_runtime->pending_request.invoke_id;
            UnitLabMmsBerElement invoke_id_element;
            UnitLabMmsBerElement service_element;
            size_t invoke_id_length = 0U;
            size_t service_encoded_length = 0U;

            do {
                invoke_id_bytes[sizeof(invoke_id_bytes) - 1U - invoke_id_length_bytes] = (uint8_t)(value & 0xFFU);
                invoke_id_length_bytes++;
                value >>= 8U;
            } while (value != 0U && invoke_id_length_bytes < sizeof(invoke_id_bytes));
            if (invoke_id_bytes[sizeof(invoke_id_bytes) - invoke_id_length_bytes] & 0x80U) {
                if (sizeof(invoke_id_bytes) == invoke_id_length_bytes) {
                    server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Confirmed response invokeID encoding failed.");
                    return 0;
                }
                invoke_id_bytes[sizeof(invoke_id_bytes) - invoke_id_length_bytes - 1U] = 0x00U;
                invoke_id_length_bytes++;
            }
            unitlab_mms_ber_element_init(&invoke_id_element);
            invoke_id_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL;
            invoke_id_element.tag.constructed = 0;
            invoke_id_element.tag.tag_number = 2U;
            invoke_id_element.value_bytes = &invoke_id_bytes[sizeof(invoke_id_bytes) - invoke_id_length_bytes];
            invoke_id_element.value_length = invoke_id_length_bytes;
            if (!unitlab_mms_ber_write(&invoke_id_element, synthesized_service_bytes, sizeof(synthesized_service_bytes), &invoke_id_length, diagnostic)) {
                return 0;
            }
            unitlab_mms_ber_element_init(&service_element);
            service_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
            service_element.tag.constructed = 0;
            service_element.tag.tag_number = 5U;
            service_element.value_bytes = NULL;
            service_element.value_length = 0U;
            if (!unitlab_mms_ber_write(&service_element, &synthesized_service_bytes[invoke_id_length], sizeof(synthesized_service_bytes) - invoke_id_length, &service_encoded_length, diagnostic)) {
                return 0;
            }
            synthesized_service_length = invoke_id_length + service_encoded_length;
            service_bytes = synthesized_service_bytes;
            service_length = synthesized_service_length;
        }
    }
    {
        const char* service_name = "Read";

        if (server_runtime->pending_request.kind == UNITLAB_MMS_REQUEST_WRITE) {
            service_name = "Write";
        } else if (server_runtime->pending_request.kind == UNITLAB_MMS_REQUEST_GET_NAME_LIST) {
            service_name = "GetNameList";
        } else if (server_runtime->pending_request.kind == UNITLAB_MMS_REQUEST_GET_VARIABLE_ACCESS_ATTRIBUTES) {
            service_name = "GetVariableAccessAttributes";
        } else if (server_runtime->pending_request.kind == UNITLAB_MMS_REQUEST_GET_NAMED_VARIABLE_LIST_ATTRIBUTES) {
            service_name = "GetNamedVariableListAttributes";
        }
        if (server_runtime->last_outgoing_summary[0] == '\0') {
            server_runtime_store_outgoing_context(server_runtime, server_runtime->pending_request.invoke_id, service_name, "status=success");
        }
        server_runtime_log_confirmed_response_preview(server_runtime, server_runtime->pending_request.invoke_id, service_name, service_bytes, service_length);
        if (!server_runtime_validate_confirmed_response_payload(service_name, service_bytes, service_length, diagnostic)) {
            return 0;
        }
    }
    if (!server_runtime_prepare_confirmed_response_pdu(server_runtime, service_bytes, service_length, &response_pdu, diagnostic)) {
        return 0;
    }

    if (!unitlab_mms_build_confirmed_response_frame(&response_pdu, server_runtime->wire_scratch, sizeof(server_runtime->wire_scratch), buffer, buffer_length, &response_length, diagnostic)) {
        return 0;
    }
    if (!unitlab_mms_transport_exchange_bind_response(&server_runtime->transport, buffer, buffer_length, diagnostic)) {
        return 0;
    }
    if (!unitlab_mms_transport_exchange_set_response_length(&server_runtime->transport, response_length, diagnostic)) {
        return 0;
    }
    *encoded_length = response_length;
    server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    unitlab_mms_operation_result_from_trace(
        &server_runtime->last_result,
        1,
        diagnostic,
        &server_runtime->transport.event_log,
        &server_runtime->transport.last_event);
    unitlab_mms_server_runtime_capture_snapshot(server_runtime);
    return 1;
}

int unitlab_mms_server_runtime_apply_incoming_bytes(UnitLabMmsServerRuntime* server_runtime, const uint8_t* buffer, size_t buffer_length, size_t* consumed_length, UnitLabMmsOperationResult* operation_result)
{
    UnitLabMmsPdu wire_pdu;
    size_t transport_consumed_length = 0U;

    if (consumed_length != NULL) {
        *consumed_length = 0U;
    }
    if (server_runtime == NULL || buffer == NULL || consumed_length == NULL || operation_result == NULL) {
        if (operation_result != NULL) {
            unitlab_mms_operation_result_init(operation_result);
            operation_result->diagnostic.code = UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT;
            snprintf(operation_result->diagnostic.message, sizeof(operation_result->diagnostic.message), "%s", "Server runtime, buffer, consumed length, and operation result are required.");
            server_runtime_fail_and_capture(server_runtime, operation_result);
        }
        return 0;
    }
    if (server_runtime->state != UNITLAB_MMS_SERVER_RUNTIME_RUNNING) {
        unitlab_mms_operation_result_init(operation_result);
        operation_result->diagnostic.code = UNITLAB_MMS_DIAGNOSTIC_BAD_STATE;
        snprintf(operation_result->diagnostic.message, sizeof(operation_result->diagnostic.message), "%s", "Server runtime must be running before applying incoming bytes.");
        server_runtime_fail_and_capture(server_runtime, operation_result);
        return 0;
    }

    unitlab_mms_operation_result_init(operation_result);
    unitlab_mms_pdu_init(&wire_pdu);
    if (!server_runtime_decode_transport_to_wire_pdu(buffer, buffer_length, &transport_consumed_length, &wire_pdu, operation_result)) {
        server_runtime_fail_and_capture(server_runtime, operation_result);
        return 0;
    }
    server_runtime->last_wire_pdu = wire_pdu;
    if (wire_pdu.kind == UNITLAB_MMS_PDU_CONFIRMED_REQUEST) {
        server_runtime_log_confirmed_request(server_runtime, &wire_pdu);
    }
    if (!unitlab_mms_transport_exchange_bind_request(
            &server_runtime->transport,
            buffer,
            transport_consumed_length,
            wire_pdu.has_invoke_id ? wire_pdu.invoke_id : 0U,
            &operation_result->diagnostic)) {
        server_runtime_fail_and_capture(server_runtime, operation_result);
        return 0;
    }
    if (!unitlab_mms_runtime_apply_wire_pdu_with_report_control(
            &server_runtime->session,
            &server_runtime->pending_request,
            &server_runtime->report_control,
            &wire_pdu,
            operation_result)) {
        server_runtime_fail_and_capture(server_runtime, operation_result);
        return 0;
    }
    server_runtime->last_result = *operation_result;
    unitlab_mms_server_runtime_capture_snapshot(server_runtime);
    *consumed_length = transport_consumed_length;
    return 1;
}

int unitlab_mms_server_runtime_apply_association_request_bytes(UnitLabMmsServerRuntime* server_runtime, const uint8_t* buffer, size_t buffer_length, size_t* consumed_length, UnitLabMmsOperationResult* operation_result)
{
    UnitLabMmsPdu wire_pdu;
    size_t transport_consumed_length = 0U;

    if (consumed_length != NULL) {
        *consumed_length = 0U;
    }
    if (server_runtime == NULL || buffer == NULL || consumed_length == NULL || operation_result == NULL) {
        if (operation_result != NULL) {
            unitlab_mms_operation_result_init(operation_result);
            operation_result->diagnostic.code = UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT;
            snprintf(operation_result->diagnostic.message, sizeof(operation_result->diagnostic.message), "%s", "Server runtime, buffer, consumed length, and operation result are required.");
            server_runtime_fail_and_capture(server_runtime, operation_result);
        }
        return 0;
    }
    if (server_runtime->state != UNITLAB_MMS_SERVER_RUNTIME_RUNNING) {
        unitlab_mms_operation_result_init(operation_result);
        operation_result->diagnostic.code = UNITLAB_MMS_DIAGNOSTIC_BAD_STATE;
        snprintf(operation_result->diagnostic.message, sizeof(operation_result->diagnostic.message), "%s", "Server runtime must be running before applying association request bytes.");
        server_runtime_fail_and_capture(server_runtime, operation_result);
        return 0;
    }

    unitlab_mms_operation_result_init(operation_result);
    unitlab_mms_pdu_init(&wire_pdu);
    if (!server_runtime_decode_transport_to_wire_pdu(buffer, buffer_length, &transport_consumed_length, &wire_pdu, operation_result)) {
        server_runtime_fail_and_capture(server_runtime, operation_result);
        return 0;
    }
    server_runtime->last_wire_pdu = wire_pdu;
    if (wire_pdu.kind != UNITLAB_MMS_PDU_INITIATE_REQUEST) {
        operation_result->diagnostic.code = UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED;
        snprintf(operation_result->diagnostic.message, sizeof(operation_result->diagnostic.message), "%s", "Association request bytes must carry an MMS initiate request.");
        server_runtime_fail_and_capture(server_runtime, operation_result);
        return 0;
    }
    if (!unitlab_mms_transport_exchange_bind_request(
            &server_runtime->transport,
            buffer,
            transport_consumed_length,
            0U,
            &operation_result->diagnostic)) {
        server_runtime_fail_and_capture(server_runtime, operation_result);
        return 0;
    }
    if (!unitlab_mms_runtime_apply_wire_pdu_with_report_control(
            &server_runtime->session,
            &server_runtime->pending_request,
            &server_runtime->report_control,
            &wire_pdu,
            operation_result)) {
        server_runtime_fail_and_capture(server_runtime, operation_result);
        return 0;
    }
    server_runtime->last_result = *operation_result;
    unitlab_mms_server_runtime_capture_snapshot(server_runtime);
    *consumed_length = transport_consumed_length;
    return 1;
}

int unitlab_mms_server_runtime_apply_association_bytes(UnitLabMmsServerRuntime* server_runtime, const uint8_t* buffer, size_t buffer_length, size_t* consumed_length, UnitLabMmsOperationResult* operation_result)
{
    return unitlab_mms_server_runtime_apply_incoming_bytes(server_runtime, buffer, buffer_length, consumed_length, operation_result);
}

int unitlab_mms_server_runtime_apply_wire_pdu(UnitLabMmsServerRuntime* server_runtime, const UnitLabMmsPdu* wire_pdu, UnitLabMmsOperationResult* operation_result)
{
    if (server_runtime == NULL) {
        return 0;
    }
    if (server_runtime->state != UNITLAB_MMS_SERVER_RUNTIME_RUNNING) {
        if (operation_result != NULL) {
            unitlab_mms_operation_result_init(operation_result);
            operation_result->diagnostic.code = UNITLAB_MMS_DIAGNOSTIC_BAD_STATE;
            snprintf(operation_result->diagnostic.message, sizeof(operation_result->diagnostic.message), "%s", "Server runtime must be running before applying wire PDUs.");
        }
        return 0;
    }
    if (wire_pdu != NULL && wire_pdu->kind == UNITLAB_MMS_PDU_CONFIRMED_REQUEST) {
        server_runtime_log_confirmed_request(server_runtime, wire_pdu);
    }
    if (!unitlab_mms_runtime_apply_wire_pdu_with_report_control(
            &server_runtime->session,
            &server_runtime->pending_request,
            &server_runtime->report_control,
            wire_pdu,
            operation_result)) {
        if (operation_result != NULL) {
            server_runtime->last_result = *operation_result;
        }
        unitlab_mms_server_runtime_capture_snapshot(server_runtime);
        return 0;
    }
    if (operation_result != NULL) {
        server_runtime->last_result = *operation_result;
    }
    unitlab_mms_server_runtime_capture_snapshot(server_runtime);
    return 1;
}

void unitlab_mms_server_runtime_capture_snapshot(UnitLabMmsServerRuntime* server_runtime)
{
    if (server_runtime == NULL) {
        return;
    }
    unitlab_mms_runtime_snapshot_capture(
        &server_runtime->snapshot,
        &server_runtime->session,
        &server_runtime->report_control,
        &server_runtime->transport,
        &server_runtime->last_result);
}
