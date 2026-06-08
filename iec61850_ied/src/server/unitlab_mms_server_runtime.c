#include "server/unitlab_mms_server_runtime.h"
#include "server/unitlab_mms_server_runtime_internal.h"

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

void server_runtime_set_diagnostic(UnitLabMmsDiagnostic* diagnostic, UnitLabMmsDiagnosticCode code, const char* message)
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

const char* server_runtime_service_name_for_kind(UnitLabMmsServiceKind service_kind)
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

const char* server_runtime_decoded_service_name_for_pdu(const UnitLabMmsPdu* wire_pdu)
{
    if (wire_pdu == NULL) {
        return "<unknown>";
    }
    return server_runtime_service_name_for_kind(wire_pdu->service_kind);
}

int server_runtime_tag_to_hex(const UnitLabMmsBerTag* tag, char* buffer, size_t buffer_length)
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

int server_runtime_bytes_are_printable_ascii(const uint8_t* bytes, size_t length)
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

void server_runtime_print_hex_bytes(const uint8_t* bytes, size_t length)
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

void server_runtime_store_incoming_context(UnitLabMmsServerRuntime* server_runtime, uint32_t invoke_id, const char* service_name)
{
    if (server_runtime == NULL) {
        return;
    }
    server_runtime->last_incoming_invoke_id = invoke_id;
    snprintf(server_runtime->last_incoming_service, sizeof(server_runtime->last_incoming_service), "%s", service_name != NULL && service_name[0] != '\0' ? service_name : "<unknown>");
}

void server_runtime_store_outgoing_context(UnitLabMmsServerRuntime* server_runtime, uint32_t invoke_id, const char* service_name, const char* summary)
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

void server_runtime_store_read_summary(UnitLabMmsServerRuntime* server_runtime, uint32_t invoke_id, int value_supported, const uint8_t* value_bytes, size_t value_length)
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

void server_runtime_store_name_list_summary(UnitLabMmsServerRuntime* server_runtime, uint32_t invoke_id, const char* service_name, const char* label, const char* const* names, size_t name_count)
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

void server_runtime_log_confirmed_response_preview(UnitLabMmsServerRuntime* server_runtime, uint32_t invoke_id, const char* service_name, const uint8_t* service_bytes, size_t service_length)
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

int server_runtime_validate_confirmed_response_payload(const char* service_name, const uint8_t* service_bytes, size_t service_length, UnitLabMmsDiagnostic* diagnostic)
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

int server_runtime_encode_ber_element(
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

int server_runtime_encode_invoke_id_element(
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

int server_runtime_parse_object_reference(const char* object_reference, char* domain_id, size_t domain_id_size, char* item_id, size_t item_id_size)
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

static void server_runtime_normalize_reference_key(const char* reference, char* buffer, size_t buffer_length)
{
    size_t offset = 0U;

    if (buffer == NULL || buffer_length == 0U) {
        return;
    }
    buffer[0] = '\0';
    if (reference == NULL) {
        return;
    }
    for (size_t index = 0U; reference[index] != '\0' && offset + 1U < buffer_length; index++) {
        char ch = reference[index];
        if (ch == '/' || ch == '$') {
            ch = '.';
        }
        buffer[offset++] = ch;
    }
    buffer[offset] = '\0';
}

int server_runtime_reference_matches_signal(const char* object_reference, const UnitLabIedModelSignal* signal)
{
    char object_key[256U];
    char signal_key[256U];

    if (object_reference == NULL || signal == NULL) {
        return 0;
    }
    server_runtime_normalize_reference_key(object_reference, object_key, sizeof(object_key));
    if (signal->object_reference[0] != '\0') {
        server_runtime_normalize_reference_key(signal->object_reference, signal_key, sizeof(signal_key));
        if (strcmp(object_key, signal_key) == 0 || strstr(object_key, signal_key) != NULL) {
            return 1;
        }
    }
    if (signal->data_set_entry_variable[0] != '\0') {
        server_runtime_normalize_reference_key(signal->data_set_entry_variable, signal_key, sizeof(signal_key));
        if (strcmp(object_key, signal_key) == 0 || strstr(object_key, signal_key) != NULL) {
            return 1;
        }
    }
    return 0;
}

const UnitLabIedModelSignal* server_runtime_find_signal_by_object_reference(const UnitLabMmsServerRuntime* server_runtime, const char* object_reference)
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

        if (server_runtime_reference_matches_signal(object_reference, signal)) {
            return signal;
        }
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

const UnitLabMmsServerRuntimeSignalValue* server_runtime_find_signal_value(const UnitLabMmsServerRuntime* server_runtime, const char* object_reference)
{
    if (server_runtime == NULL || object_reference == NULL || object_reference[0] == '\0') {
        return NULL;
    }
    for (size_t index = 0U; index < server_runtime->signal_value_count; index++) {
        const UnitLabMmsServerRuntimeSignalValue* value = &server_runtime->signal_values[index];
        UnitLabIedModelSignal signal;

        if (value->in_use == 0) {
            continue;
        }
        memset(&signal, 0, sizeof(signal));
        snprintf(signal.object_reference, sizeof(signal.object_reference), "%s", value->object_reference);
        snprintf(signal.data_set_entry_variable, sizeof(signal.data_set_entry_variable), "%s", value->data_set_entry_variable);
        if (server_runtime_reference_matches_signal(object_reference, &signal)) {
            return value;
        }
    }
    return NULL;
}

int server_runtime_parse_int32_value(const char* source, int32_t* value)
{
    char* end = NULL;
    long parsed = strtol(source, &end, 10);
    if (source == NULL || value == NULL || source == end || end == NULL || *end != '\0' || parsed < INT32_MIN || parsed > INT32_MAX) {
        return 0;
    }
    *value = (int32_t)parsed;
    return 1;
}

int server_runtime_encode_signed_integer(int32_t value, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic)
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

int server_runtime_encode_mms_data_value(
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
            element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
            element.tag.constructed = 0;
            element.tag.tag_number = 3U;
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
            element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
            element.tag.constructed = 0;
            element.tag.tag_number = 5U;
            element.value_bytes = integer_bytes;
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


static int server_runtime_encode_signal_write_value(
    const UnitLabIedModelSignal* signal,
    const uint8_t* value_bytes,
    size_t value_length,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    uint32_t tag_number = 0U;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (signal == NULL || value_bytes == NULL || value_length == 0U || buffer == NULL || encoded_length == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Signal update requires a signal, value bytes, and output buffer.");
        return 0;
    }
    switch (signal->initial_value_kind) {
        case UNITLAB_IED_FIXTURE_VALUE_BOOLEAN:
            tag_number = 3U;
            break;
        case UNITLAB_IED_FIXTURE_VALUE_INTEGER:
            tag_number = 5U;
            break;
        case UNITLAB_IED_FIXTURE_VALUE_STRING:
            tag_number = 10U;
            break;
        default:
            server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "Signal update value kind is unsupported.");
            return 0;
    }
    return server_runtime_encode_ber_element(
        UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
        0,
        tag_number,
        value_bytes,
        value_length,
        buffer,
        buffer_length,
        encoded_length,
        diagnostic);
}

int server_runtime_encode_current_signal_value(
    const UnitLabMmsServerRuntime* server_runtime,
    const UnitLabIedModelSignal* signal,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    const UnitLabMmsServerRuntimeSignalValue* runtime_value = NULL;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (server_runtime == NULL || signal == NULL || buffer == NULL || encoded_length == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Current signal value encoding requires runtime, signal, and output buffer.");
        return 0;
    }
    runtime_value = server_runtime_find_signal_value(server_runtime, signal->data_set_entry_variable[0] != '\0' ? signal->data_set_entry_variable : signal->object_reference);
    if (runtime_value != NULL && runtime_value->encoded_value_length != 0U) {
        if (runtime_value->encoded_value_length > buffer_length) {
            server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Current signal value buffer is too small.");
            return 0;
        }
        memcpy(buffer, runtime_value->encoded_value, runtime_value->encoded_value_length);
        *encoded_length = runtime_value->encoded_value_length;
        return 1;
    }
    return server_runtime_encode_mms_data_value(signal, buffer, buffer_length, encoded_length, diagnostic);
}

static int server_runtime_encode_context_data(
    uint32_t tag_number,
    const uint8_t* value_bytes,
    size_t value_length,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    return server_runtime_encode_ber_element(
        UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
        0,
        tag_number,
        value_bytes,
        value_length,
        buffer,
        buffer_length,
        encoded_length,
        diagnostic);
}

static int server_runtime_replace_trailing_component(const char* reference, const char* replacement, char* buffer, size_t buffer_length)
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

static const UnitLabMmsServerRuntimeSignalValue* server_runtime_find_signal_metadata_value(const UnitLabMmsServerRuntime* server_runtime, const UnitLabIedModelSignal* signal)
{
    const UnitLabMmsServerRuntimeSignalValue* runtime_value = NULL;
    const char* reference = NULL;
    char value_reference[256U];

    if (server_runtime == NULL || signal == NULL) {
        return NULL;
    }
    reference = signal->data_set_entry_variable[0] != '\0' ? signal->data_set_entry_variable : signal->object_reference;
    if (strcmp(signal->data_attribute_path, "q") != 0 && strcmp(signal->data_attribute_path, "t") != 0) {
        return server_runtime_find_signal_value(server_runtime, reference);
    }
    if (server_runtime_replace_trailing_component(reference, "stVal", value_reference, sizeof(value_reference))) {
        runtime_value = server_runtime_find_signal_value(server_runtime, value_reference);
        if (runtime_value != NULL) {
            return runtime_value;
        }
    }
    if (server_runtime_replace_trailing_component(reference, "f", value_reference, sizeof(value_reference))) {
        runtime_value = server_runtime_find_signal_value(server_runtime, value_reference);
        if (runtime_value != NULL) {
            return runtime_value;
        }
    }
    return server_runtime_find_signal_value(server_runtime, reference);
}

int server_runtime_encode_current_signal_quality(
    const UnitLabMmsServerRuntime* server_runtime,
    const UnitLabIedModelSignal* signal,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    const UnitLabMmsServerRuntimeSignalValue* runtime_value = NULL;
    const uint8_t default_quality[2U] = { 0x00U, 0x00U };
    const uint8_t* quality_bytes = default_quality;
    size_t quality_length = sizeof(default_quality);

    if (server_runtime == NULL || signal == NULL || buffer == NULL || encoded_length == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Current signal quality encoding requires runtime, signal, and output buffer.");
        return 0;
    }
    runtime_value = server_runtime_find_signal_metadata_value(server_runtime, signal);
    if (runtime_value != NULL && runtime_value->quality_value_length != 0U) {
        quality_bytes = runtime_value->quality_value;
        quality_length = runtime_value->quality_value_length;
    }
    return server_runtime_encode_context_data(4U, quality_bytes, quality_length, buffer, buffer_length, encoded_length, diagnostic);
}

int server_runtime_encode_current_signal_timestamp(
    const UnitLabMmsServerRuntime* server_runtime,
    const UnitLabIedModelSignal* signal,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    const UnitLabMmsServerRuntimeSignalValue* runtime_value = NULL;
    const uint8_t default_timestamp[6U] = { 0U, 0U, 0U, 0U, 0U, 0U };
    const uint8_t* timestamp_bytes = default_timestamp;
    size_t timestamp_length = sizeof(default_timestamp);

    if (server_runtime == NULL || signal == NULL || buffer == NULL || encoded_length == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Current signal timestamp encoding requires runtime, signal, and output buffer.");
        return 0;
    }
    runtime_value = server_runtime_find_signal_metadata_value(server_runtime, signal);
    if (runtime_value != NULL && runtime_value->timestamp_value_length != 0U) {
        timestamp_bytes = runtime_value->timestamp_value;
        timestamp_length = runtime_value->timestamp_value_length;
    }
    return server_runtime_encode_context_data(12U, timestamp_bytes, timestamp_length, buffer, buffer_length, encoded_length, diagnostic);
}

static void server_runtime_reset_signal_values(UnitLabMmsServerRuntime* server_runtime)
{
    if (server_runtime == NULL) {
        return;
    }
    memset(server_runtime->signal_values, 0, sizeof(server_runtime->signal_values));
    server_runtime->signal_value_count = 0U;
}

static int server_runtime_seed_signal_values(UnitLabMmsServerRuntime* server_runtime, UnitLabMmsDiagnostic* diagnostic)
{
    if (server_runtime == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Server runtime is required to seed signal values.");
        return 0;
    }
    server_runtime_reset_signal_values(server_runtime);
    if (server_runtime->model_plan == NULL || server_runtime->model_plan->signals == NULL) {
        return 1;
    }
    for (size_t index = 0U; index < server_runtime->model_plan->signal_count; index++) {
        UnitLabMmsServerRuntimeSignalValue* runtime_value = NULL;
        const UnitLabIedModelSignal* signal = &server_runtime->model_plan->signals[index];
        size_t encoded_length = 0U;

        if (server_runtime->signal_value_count >= UNITLAB_MMS_SERVER_RUNTIME_MAX_SIGNAL_VALUES) {
            server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Runtime signal value store is too small for the model plan.");
            return 0;
        }
        runtime_value = &server_runtime->signal_values[server_runtime->signal_value_count];
        if (!server_runtime_encode_mms_data_value(signal, runtime_value->encoded_value, sizeof(runtime_value->encoded_value), &encoded_length, diagnostic)) {
            if (diagnostic != NULL && diagnostic->code != UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED) {
                return 0;
            }
            unitlab_mms_diagnostic_clear(diagnostic);
            continue;
        }
        runtime_value->in_use = 1;
        snprintf(runtime_value->object_reference, sizeof(runtime_value->object_reference), "%s", signal->object_reference);
        snprintf(runtime_value->data_set_entry_variable, sizeof(runtime_value->data_set_entry_variable), "%s", signal->data_set_entry_variable);
        runtime_value->encoded_value_length = encoded_length;
        server_runtime->signal_value_count++;
    }
    server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}

static UnitLabMmsServerRuntimeSignalValue* server_runtime_find_signal_value_mutable(UnitLabMmsServerRuntime* server_runtime, const char* object_reference)
{
    if (server_runtime == NULL || object_reference == NULL || object_reference[0] == '\0') {
        return NULL;
    }
    for (size_t index = 0U; index < server_runtime->signal_value_count; index++) {
        UnitLabMmsServerRuntimeSignalValue* value = &server_runtime->signal_values[index];
        UnitLabIedModelSignal signal;

        if (value->in_use == 0) {
            continue;
        }
        memset(&signal, 0, sizeof(signal));
        snprintf(signal.object_reference, sizeof(signal.object_reference), "%s", value->object_reference);
        snprintf(signal.data_set_entry_variable, sizeof(signal.data_set_entry_variable), "%s", value->data_set_entry_variable);
        if (server_runtime_reference_matches_signal(object_reference, &signal)) {
            return value;
        }
    }
    return NULL;
}

static uint8_t server_runtime_report_trigger_options_mask_or_default(const UnitLabIedModelReportControl* report)
{
    if (report == NULL) {
        return 0U;
    }
    if (report->trigger_options_mask == 0U) {
        return UNITLAB_IED_MODEL_TRG_OPT_DATA_CHANGED | UNITLAB_IED_MODEL_TRG_OPT_GI;
    }
    return report->trigger_options_mask;
}

static int server_runtime_report_data_change_trigger_enabled(const UnitLabIedModelReportControl* report)
{
    return (server_runtime_report_trigger_options_mask_or_default(report) & UNITLAB_IED_MODEL_TRG_OPT_DATA_CHANGED) != 0U;
}

static int server_runtime_report_quality_change_trigger_enabled(const UnitLabIedModelReportControl* report)
{
    return (server_runtime_report_trigger_options_mask_or_default(report) & UNITLAB_IED_MODEL_TRG_OPT_QUALITY_CHANGED) != 0U;
}

static int server_runtime_report_data_update_trigger_enabled(const UnitLabIedModelReportControl* report)
{
    return (server_runtime_report_trigger_options_mask_or_default(report) & UNITLAB_IED_MODEL_TRG_OPT_DATA_UPDATE) != 0U;
}

static int server_runtime_report_integrity_trigger_enabled(const UnitLabIedModelReportControl* report)
{
    return (server_runtime_report_trigger_options_mask_or_default(report) & UNITLAB_IED_MODEL_TRG_OPT_INTEGRITY) != 0U;
}

static uint32_t server_runtime_report_integrity_period_ms_or_default(const UnitLabIedModelReportControl* report)
{
    return report != NULL && report->integrity_period_ms_known ? report->integrity_period_ms : 0U;
}

static int server_runtime_reference_with_replaced_component_matches_signal(const char* object_reference, const char* component, const UnitLabIedModelSignal* signal)
{
    char candidate_reference[256U];

    if (!server_runtime_replace_trailing_component(object_reference, component, candidate_reference, sizeof(candidate_reference))) {
        return 0;
    }
    return server_runtime_reference_matches_signal(candidate_reference, signal);
}

void server_runtime_clear_pending_reports(UnitLabMmsServerRuntime* server_runtime)
{
    if (server_runtime == NULL) {
        return;
    }
    server_runtime->pending_gi_report = 0U;
    server_runtime->next_integrity_report_ms = 0U;
    server_runtime->pending_report_kind = UNITLAB_MMS_SERVER_PENDING_REPORT_NONE;
    server_runtime->pending_report_member_index = 0U;
    server_runtime->pending_report_member_mask = 0U;
    server_runtime->pending_report_value_length = 0U;
    memset(server_runtime->pending_report_values, 0, sizeof(server_runtime->pending_report_values));
    memset(server_runtime->pending_report_value_lengths, 0, sizeof(server_runtime->pending_report_value_lengths));
    memset(server_runtime->pending_report_queue, 0, sizeof(server_runtime->pending_report_queue));
    server_runtime->pending_report_queue_count = 0U;
    server_runtime->brcb_buffer_overflow = 0U;
}

void server_runtime_advance_pending_report_queue(UnitLabMmsServerRuntime* server_runtime)
{
    UnitLabMmsServerPendingReportEntry next_entry;

    if (server_runtime == NULL) {
        return;
    }
    if (server_runtime->pending_report_queue_count == 0U) {
        server_runtime_clear_pending_reports(server_runtime);
        return;
    }
    next_entry = server_runtime->pending_report_queue[0];
    for (size_t index = 1U; index < server_runtime->pending_report_queue_count; index++) {
        server_runtime->pending_report_queue[index - 1U] = server_runtime->pending_report_queue[index];
    }
    server_runtime->pending_report_queue_count--;
    memset(&server_runtime->pending_report_queue[server_runtime->pending_report_queue_count], 0, sizeof(server_runtime->pending_report_queue[server_runtime->pending_report_queue_count]));
    server_runtime->pending_gi_report = 1U;
    server_runtime->pending_report_kind = next_entry.kind;
    server_runtime->pending_report_member_index = next_entry.member_index;
    server_runtime->pending_report_member_mask = next_entry.member_mask;
    memset(server_runtime->pending_report_values, 0, sizeof(server_runtime->pending_report_values));
    memset(server_runtime->pending_report_value_lengths, 0, sizeof(server_runtime->pending_report_value_lengths));
    memcpy(server_runtime->pending_report_values, next_entry.values, sizeof(server_runtime->pending_report_values));
    memcpy(server_runtime->pending_report_value_lengths, next_entry.value_lengths, sizeof(server_runtime->pending_report_value_lengths));
    if (next_entry.member_index < UNITLAB_MMS_SERVER_RUNTIME_MAX_REPORT_MEMBERS) {
        memcpy(server_runtime->pending_report_value, next_entry.values[next_entry.member_index], next_entry.value_lengths[next_entry.member_index]);
        server_runtime->pending_report_value_length = next_entry.value_lengths[next_entry.member_index];
    } else {
        server_runtime->pending_report_value_length = 0U;
    }
}

int server_runtime_queue_pending_report_event(UnitLabMmsServerRuntime* server_runtime, UnitLabMmsServerPendingReportKind kind, size_t member_index, const uint8_t* value, size_t value_length)
{
    uint64_t member_mask = member_index < 64U ? ((uint64_t)1U << member_index) : 0U;

    if (server_runtime == NULL || kind == UNITLAB_MMS_SERVER_PENDING_REPORT_NONE || member_mask == 0U || value == NULL || value_length == 0U
        || member_index >= UNITLAB_MMS_SERVER_RUNTIME_MAX_REPORT_MEMBERS
        || value_length > UNITLAB_MMS_SERVER_RUNTIME_SIGNAL_VALUE_LENGTH) {
        return 0;
    }
    if (server_runtime->pending_report_kind == UNITLAB_MMS_SERVER_PENDING_REPORT_NONE) {
        server_runtime->pending_gi_report = 1U;
        server_runtime->pending_report_kind = kind;
        server_runtime->pending_report_member_index = member_index;
        server_runtime->pending_report_member_mask = member_mask;
        memcpy(server_runtime->pending_report_value, value, value_length);
        server_runtime->pending_report_value_length = value_length;
        memcpy(server_runtime->pending_report_values[member_index], value, value_length);
        server_runtime->pending_report_value_lengths[member_index] = value_length;
        return 1;
    }
    if (server_runtime->pending_report_kind == kind) {
        server_runtime->pending_report_member_index = member_index;
        server_runtime->pending_report_member_mask |= member_mask;
        memcpy(server_runtime->pending_report_value, value, value_length);
        server_runtime->pending_report_value_length = value_length;
        memcpy(server_runtime->pending_report_values[member_index], value, value_length);
        server_runtime->pending_report_value_lengths[member_index] = value_length;
        return 1;
    }
    for (size_t index = 0U; index < server_runtime->pending_report_queue_count; index++) {
        UnitLabMmsServerPendingReportEntry* entry = &server_runtime->pending_report_queue[index];
        if (entry->kind == kind) {
            entry->member_index = member_index;
            entry->member_mask |= member_mask;
            memcpy(entry->values[member_index], value, value_length);
            entry->value_lengths[member_index] = value_length;
            return 1;
        }
    }
    if (server_runtime->pending_report_queue_count >= UNITLAB_MMS_SERVER_RUNTIME_MAX_PENDING_REPORTS) {
        server_runtime->brcb_buffer_overflow = 1U;
        return 0;
    }
    server_runtime->pending_report_queue[server_runtime->pending_report_queue_count] = (UnitLabMmsServerPendingReportEntry){
        .kind = kind,
        .member_index = member_index,
        .member_mask = member_mask,
    };
    memcpy(server_runtime->pending_report_queue[server_runtime->pending_report_queue_count].values[member_index], value, value_length);
    server_runtime->pending_report_queue[server_runtime->pending_report_queue_count].value_lengths[member_index] = value_length;
    server_runtime->pending_report_queue_count++;
    return 1;
}

static int server_runtime_queue_quality_change_report_member(UnitLabMmsServerRuntime* server_runtime, const char* value_leaf_reference, const uint8_t* quality_value, size_t quality_value_length)
{
    const UnitLabIedModelReportControl* report = NULL;
    const UnitLabIedModelDataSet* data_set = NULL;
    uint8_t encoded_quality[16U];
    size_t encoded_quality_length = 0U;

    if (server_runtime == NULL || value_leaf_reference == NULL || quality_value == NULL || quality_value_length == 0U || server_runtime->brcb_rpt_ena == 0U
        || server_runtime->model_plan == NULL || server_runtime->model_plan->report_count == 0U || server_runtime->model_plan->reports == NULL) {
        return 0;
    }
    report = &server_runtime->model_plan->reports[0];
    if (!server_runtime_report_quality_change_trigger_enabled(report)
        || report->data_set_index >= server_runtime->model_plan->data_set_count
        || server_runtime->model_plan->data_sets == NULL
        || server_runtime->model_plan->signals == NULL) {
        return 0;
    }
    if (!server_runtime_encode_context_data(4U, quality_value, quality_value_length, encoded_quality, sizeof(encoded_quality), &encoded_quality_length, NULL)) {
        return 0;
    }
    data_set = &server_runtime->model_plan->data_sets[report->data_set_index];
    for (size_t index = 0U; index < data_set->member_count; index++) {
        size_t signal_index = data_set->first_signal_index + index;
        const UnitLabIedModelSignal* candidate = NULL;

        if (signal_index >= server_runtime->model_plan->signal_count) {
            break;
        }
        candidate = &server_runtime->model_plan->signals[signal_index];
        if (strcmp(candidate->data_attribute_path, "q") == 0
            && server_runtime_reference_with_replaced_component_matches_signal(value_leaf_reference, "q", candidate)) {
            return server_runtime_queue_pending_report_event(server_runtime, UNITLAB_MMS_SERVER_PENDING_REPORT_QUALITY_CHANGE, index, encoded_quality, encoded_quality_length);
        }
    }
    return 0;
}

int server_runtime_poll_integrity_report(UnitLabMmsServerRuntime* server_runtime, uint64_t now_ms, UnitLabMmsDiagnostic* diagnostic)
{
    const UnitLabIedModelReportControl* report = NULL;
    uint32_t interval_ms = 0U;

    if (server_runtime == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Server runtime is required for integrity report polling.");
        return 0;
    }
    if (server_runtime->brcb_rpt_ena == 0U || server_runtime->pending_gi_report != 0U
        || server_runtime->model_plan == NULL || server_runtime->model_plan->report_count == 0U || server_runtime->model_plan->reports == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
        return 1;
    }
    report = &server_runtime->model_plan->reports[0];
    interval_ms = server_runtime_report_integrity_period_ms_or_default(report);
    if (!server_runtime_report_integrity_trigger_enabled(report) || interval_ms == 0U) {
        server_runtime->next_integrity_report_ms = 0U;
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
        return 1;
    }
    if (server_runtime->next_integrity_report_ms == 0U) {
        server_runtime->next_integrity_report_ms = now_ms + (uint64_t)interval_ms;
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
        return 1;
    }
    if (now_ms < server_runtime->next_integrity_report_ms) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
        return 1;
    }
    server_runtime->pending_gi_report = 1U;
    server_runtime->pending_report_kind = UNITLAB_MMS_SERVER_PENDING_REPORT_INTEGRITY;
    server_runtime->pending_report_member_index = 0U;
    server_runtime->pending_report_member_mask = 0U;
    server_runtime->pending_report_value_length = 1U;
    server_runtime->next_integrity_report_ms = now_ms + (uint64_t)interval_ms;
    server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}

int unitlab_mms_server_runtime_update_signal_value(
    UnitLabMmsServerRuntime* server_runtime,
    const char* object_reference,
    const uint8_t* value_bytes,
    size_t value_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    const UnitLabIedModelSignal* signal = NULL;
    UnitLabMmsServerRuntimeSignalValue* runtime_value = NULL;
    uint8_t encoded_value[UNITLAB_MMS_SERVER_RUNTIME_SIGNAL_VALUE_LENGTH];
    size_t encoded_value_length = 0U;
    size_t member_index = 0U;
    int report_member = 0;
    int value_changed = 1;
    UnitLabMmsServerPendingReportKind queued_kind = UNITLAB_MMS_SERVER_PENDING_REPORT_NONE;

    if (server_runtime == NULL || object_reference == NULL || value_bytes == NULL || value_length == 0U) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Signal update requires runtime, object reference, and value bytes.");
        return 0;
    }
    signal = server_runtime_find_signal_by_object_reference(server_runtime, object_reference);
    if (signal == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "Signal update object is not present in the model plan.");
        return 0;
    }
    if (!server_runtime_encode_signal_write_value(signal, value_bytes, value_length, encoded_value, sizeof(encoded_value), &encoded_value_length, diagnostic)) {
        return 0;
    }
    for (size_t index = 0U; index < server_runtime->signal_value_count; index++) {
        UnitLabMmsServerRuntimeSignalValue* candidate = &server_runtime->signal_values[index];
        if (candidate->in_use == 0) {
            continue;
        }
        if (server_runtime_reference_matches_signal(object_reference, signal)
            && ((signal->data_set_entry_variable[0] != '\0' && strcmp(candidate->data_set_entry_variable, signal->data_set_entry_variable) == 0)
                || (signal->object_reference[0] != '\0' && strcmp(candidate->object_reference, signal->object_reference) == 0))) {
            runtime_value = candidate;
            break;
        }
    }
    if (runtime_value != NULL && runtime_value->encoded_value_length == encoded_value_length
        && memcmp(runtime_value->encoded_value, encoded_value, encoded_value_length) == 0) {
        value_changed = 0;
    }
    if (runtime_value == NULL) {
        if (server_runtime->signal_value_count >= UNITLAB_MMS_SERVER_RUNTIME_MAX_SIGNAL_VALUES) {
            server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Runtime signal value store is full.");
            return 0;
        }
        runtime_value = &server_runtime->signal_values[server_runtime->signal_value_count++];
        memset(runtime_value, 0, sizeof(*runtime_value));
        runtime_value->in_use = 1;
        snprintf(runtime_value->object_reference, sizeof(runtime_value->object_reference), "%s", signal->object_reference);
        snprintf(runtime_value->data_set_entry_variable, sizeof(runtime_value->data_set_entry_variable), "%s", signal->data_set_entry_variable);
    }
    memcpy(runtime_value->encoded_value, encoded_value, encoded_value_length);
    runtime_value->encoded_value_length = encoded_value_length;

    if (server_runtime->brcb_rpt_ena != 0U && server_runtime->model_plan != NULL && server_runtime->model_plan->report_count != 0U && server_runtime->model_plan->reports != NULL) {
        const UnitLabIedModelReportControl* report = &server_runtime->model_plan->reports[0];
        const UnitLabIedModelDataSet* data_set = NULL;
        if (value_changed && server_runtime_report_data_change_trigger_enabled(report)) {
            queued_kind = UNITLAB_MMS_SERVER_PENDING_REPORT_DATA_CHANGE;
        } else if (server_runtime_report_data_update_trigger_enabled(report)) {
            queued_kind = UNITLAB_MMS_SERVER_PENDING_REPORT_DATA_UPDATE;
        }
        if (queued_kind != UNITLAB_MMS_SERVER_PENDING_REPORT_NONE && report->data_set_index < server_runtime->model_plan->data_set_count && server_runtime->model_plan->data_sets != NULL) {
            data_set = &server_runtime->model_plan->data_sets[report->data_set_index];
        }
        if (data_set != NULL && server_runtime->model_plan->signals != NULL) {
            for (size_t index = 0U; index < data_set->member_count; index++) {
                size_t signal_index = data_set->first_signal_index + index;
                if (signal_index >= server_runtime->model_plan->signal_count) {
                    break;
                }
                if (server_runtime_reference_matches_signal(object_reference, &server_runtime->model_plan->signals[signal_index])) {
                    member_index = index;
                    report_member = 1;
                    break;
                }
            }
        }
    }
    if (report_member != 0) {
        if (encoded_value_length > sizeof(server_runtime->pending_report_value)) {
            server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Pending report value buffer is too small.");
            return 0;
        }
        memcpy(server_runtime->pending_report_value, encoded_value, encoded_value_length);
        server_runtime->pending_report_value_length = encoded_value_length;
        (void)server_runtime_queue_pending_report_event(server_runtime, queued_kind, member_index, encoded_value, encoded_value_length);
    }
    server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}

int unitlab_mms_server_runtime_update_signal_int32(
    UnitLabMmsServerRuntime* server_runtime,
    const char* object_reference,
    int32_t value,
    UnitLabMmsDiagnostic* diagnostic)
{
    uint8_t value_bytes[5U];
    size_t value_length = 0U;

    if (!server_runtime_encode_signed_integer(value, value_bytes, sizeof(value_bytes), &value_length, diagnostic)) {
        return 0;
    }
    return unitlab_mms_server_runtime_update_signal_value(server_runtime, object_reference, value_bytes, value_length, diagnostic);
}

int unitlab_mms_server_runtime_update_signal_boolean(
    UnitLabMmsServerRuntime* server_runtime,
    const char* object_reference,
    int value,
    UnitLabMmsDiagnostic* diagnostic)
{
    uint8_t value_byte = value != 0 ? 0xFFU : 0x00U;
    return unitlab_mms_server_runtime_update_signal_value(server_runtime, object_reference, &value_byte, 1U, diagnostic);
}

int unitlab_mms_server_runtime_update_signal_visible_string(
    UnitLabMmsServerRuntime* server_runtime,
    const char* object_reference,
    const char* value,
    UnitLabMmsDiagnostic* diagnostic)
{
    if (value == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Visible-string signal update requires a value.");
        return 0;
    }
    return unitlab_mms_server_runtime_update_signal_value(server_runtime, object_reference, (const uint8_t*)value, strlen(value), diagnostic);
}

int unitlab_mms_server_runtime_update_signal_quality(
    UnitLabMmsServerRuntime* server_runtime,
    const char* value_leaf_reference,
    const uint8_t* quality_bytes,
    size_t quality_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsServerRuntimeSignalValue* runtime_value = NULL;
    const UnitLabIedModelSignal* signal = NULL;

    if (server_runtime == NULL || value_leaf_reference == NULL || quality_bytes == NULL || quality_length == 0U) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Signal quality update requires runtime, value leaf reference, and quality bytes.");
        return 0;
    }
    if (quality_length > sizeof(((UnitLabMmsServerRuntimeSignalValue*)0)->quality_value)) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Signal quality value is too large.");
        return 0;
    }
    signal = server_runtime_find_signal_by_object_reference(server_runtime, value_leaf_reference);
    if (signal == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "Signal quality update value leaf is not present in the model plan.");
        return 0;
    }
    runtime_value = server_runtime_find_signal_value_mutable(server_runtime, signal->data_set_entry_variable[0] != '\0' ? signal->data_set_entry_variable : signal->object_reference);
    if (runtime_value == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "Signal quality update could not resolve a runtime value slot.");
        return 0;
    }
    memcpy(runtime_value->quality_value, quality_bytes, quality_length);
    runtime_value->quality_value_length = quality_length;
    (void)server_runtime_queue_quality_change_report_member(server_runtime, value_leaf_reference, runtime_value->quality_value, runtime_value->quality_value_length);
    server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}

int unitlab_mms_server_runtime_update_signal_timestamp(
    UnitLabMmsServerRuntime* server_runtime,
    const char* value_leaf_reference,
    const uint8_t* timestamp_bytes,
    size_t timestamp_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsServerRuntimeSignalValue* runtime_value = NULL;
    const UnitLabIedModelSignal* signal = NULL;

    if (server_runtime == NULL || value_leaf_reference == NULL || timestamp_bytes == NULL || timestamp_length == 0U) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Signal timestamp update requires runtime, value leaf reference, and timestamp bytes.");
        return 0;
    }
    if (timestamp_length > sizeof(((UnitLabMmsServerRuntimeSignalValue*)0)->timestamp_value)) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Signal timestamp value is too large.");
        return 0;
    }
    signal = server_runtime_find_signal_by_object_reference(server_runtime, value_leaf_reference);
    if (signal == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "Signal timestamp update value leaf is not present in the model plan.");
        return 0;
    }
    runtime_value = server_runtime_find_signal_value_mutable(server_runtime, signal->data_set_entry_variable[0] != '\0' ? signal->data_set_entry_variable : signal->object_reference);
    if (runtime_value == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "Signal timestamp update could not resolve a runtime value slot.");
        return 0;
    }
    memcpy(runtime_value->timestamp_value, timestamp_bytes, timestamp_length);
    runtime_value->timestamp_value_length = timestamp_length;
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
    if (!server_runtime_seed_signal_values(server_runtime, NULL)) {
        return 0;
    }
    unitlab_mms_server_runtime_capture_snapshot(server_runtime);
    return 1;
}


int server_runtime_object_reference_has_suffix(const char* object_reference, const char* suffix)
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

int server_runtime_object_reference_has_suffix_any(
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

int server_runtime_object_reference_matches_report_control_field(const char* object_reference, const char* field_name)
{
    char br_suffix[128U];
    char legacy_br_suffix[160U];
    char no_br_suffix[128U];
    char legacy_no_br_suffix[160U];

    if (object_reference == NULL || field_name == NULL || field_name[0] == '\0') {
        return 0;
    }
    snprintf(br_suffix, sizeof(br_suffix), ".BR.brcbEvents.%s", field_name);
    snprintf(legacy_br_suffix, sizeof(legacy_br_suffix), ".BR.LLN0_Events_BuffRep01.%s", field_name);
    snprintf(no_br_suffix, sizeof(no_br_suffix), ".brcbEvents.%s", field_name);
    snprintf(legacy_no_br_suffix, sizeof(legacy_no_br_suffix), ".LLN0_Events_BuffRep01.%s", field_name);
    return server_runtime_object_reference_has_suffix_any(
        object_reference,
        (const char* const[]){ br_suffix, legacy_br_suffix, no_br_suffix, legacy_no_br_suffix },
        4U);
}

int server_runtime_object_reference_matches_report_control_object(const char* object_reference)
{
    if (object_reference == NULL) {
        return 0;
    }
    return server_runtime_object_reference_has_suffix_any(
        object_reference,
        (const char* const[]){ ".BR.brcbEvents", ".BR.LLN0_Events_BuffRep01", ".brcbEvents", ".LLN0_Events_BuffRep01" },
        4U);
}

const char* server_runtime_advertised_domain_name(const UnitLabMmsServerRuntime* server_runtime)
{
    if (server_runtime != NULL
        && server_runtime->model_plan != NULL
        && server_runtime->model_plan->logical_devices != NULL
        && server_runtime->model_plan->logical_device_count > 0U
        && server_runtime->model_plan->logical_devices[0].inst[0] != '\0') {
        return server_runtime->model_plan->logical_devices[0].inst;
    }
    return "IED1LD0";
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
    server_runtime->brcb_rpt_ena = 0U;
    server_runtime->brcb_resv_tms = 0U;
    server_runtime->brcb_sq_num = 0U;
    server_runtime->brcb_entry_id_counter = 0U;
    memset(server_runtime->brcb_entry_id, 0, sizeof(server_runtime->brcb_entry_id));
    memset(server_runtime->brcb_time_of_entry, 0, sizeof(server_runtime->brcb_time_of_entry));
    server_runtime->brcb_buffer_overflow = 0U;
    server_runtime_clear_pending_reports(server_runtime);
    memset(server_runtime->pending_report_value, 0, sizeof(server_runtime->pending_report_value));
    memset(server_runtime->signal_values, 0, sizeof(server_runtime->signal_values));
    server_runtime->signal_value_count = 0U;
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

int unitlab_mms_server_runtime_build_release_response_bytes(UnitLabMmsServerRuntime* server_runtime, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsPdu response_pdu;
    size_t response_length = 0U;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (server_runtime == NULL || buffer == NULL || encoded_length == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Server runtime, buffer, and encoded_length are required.");
        return 0;
    }
    if (server_runtime->state != UNITLAB_MMS_SERVER_RUNTIME_RUNNING) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BAD_STATE, "Server runtime must be running before building release response bytes.");
        return 0;
    }
    if (server_runtime->session.state != UNITLAB_MMS_SESSION_RELEASING) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BAD_STATE, "Release response requires a releasing session.");
        return 0;
    }

    unitlab_mms_pdu_init(&response_pdu);
    response_pdu.kind = UNITLAB_MMS_PDU_CONCLUDE_RESPONSE;
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
    server_runtime_store_outgoing_context(server_runtime, 0U, "Conclude", "status=success release-response");
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
