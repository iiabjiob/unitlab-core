#define _POSIX_C_SOURCE 200112L
#include "native_wire_server.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <signal.h>
#include <math.h>
#include <stdint.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "model/model_loader.h"
#include "server/unitlab_mms_server_runtime_internal.h"
#include "wire/mms/unitlab_mms_pdu.h"
#include "wire/transport/unitlab_mms_transport_frame.h"
#include "wire/orchestration/unitlab_mms_live_wire_probe.h"
#include "wire/orchestration/unitlab_mms_wire_builder.h"
#include "protocols/mms/unitlab_mms_wire_semantic_bridge.h"
static void set_result(UnitLabIedModelLoadResult* result, const char* code, const char* message)
{
    if (result == NULL) {
        return;
    }
    result->loaded = 0;
    snprintf(result->code, sizeof(result->code), "%s", code);
    snprintf(result->message, sizeof(result->message), "%s", message);
}
static void close_fd(int* fd)
{
    if (fd == NULL || *fd < 0) {
        return;
    }
    close(*fd);
    *fd = -1;
}
static void log_native_wire_disconnect(const UnitLabMmsServerRuntime* server_runtime, const char* reason)
{
    if (server_runtime == NULL) {
        return;
    }
    printf(
        "native-wire-server: %s last-invoke=%u last-incoming-service=%s last-outgoing-service=%s last-outgoing-summary=%s\n",
        reason != NULL && reason[0] != '\0' ? reason : "client-disconnected",
        (unsigned)server_runtime->last_incoming_invoke_id,
        server_runtime->last_incoming_service[0] != '\0' ? server_runtime->last_incoming_service : "<none>",
        server_runtime->last_outgoing_service[0] != '\0' ? server_runtime->last_outgoing_service : "<none>",
        server_runtime->last_outgoing_summary[0] != '\0' ? server_runtime->last_outgoing_summary : "<none>");
    fflush(stdout);
}

static void log_native_wire_data_recv_disconnect(const UnitLabMmsServerRuntime* server_runtime, ssize_t received)
{
    if (received == 0) {
        printf("native-wire-server: data-client-recv-return=0 peer-eof client-closed\n");
    } else if (received < 0) {
        printf(
            "native-wire-server: data-client-recv-return=%zd errno=%d strerror=%s\n",
            received,
            errno,
            strerror(errno));
    } else {
        printf("native-wire-server: data-client-recv-return=%zd\n", received);
    }
    fflush(stdout);
    log_native_wire_disconnect(server_runtime, "data-client-disconnected");
}


static uint64_t native_wire_now_ms(void);

static int send_all(int fd, const uint8_t* buffer, size_t length)
{
    size_t offset = 0U;
    while (offset < length) {
        ssize_t written = send(fd, buffer + offset, length - offset, 0);
        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            return 0;
        }
        if (written == 0) {
            return 0;
        }
        offset += (size_t)written;
    }
    return 1;
}

/* Keep the next MMS response out of the same TCP payload as the final COTP segment. */
static void pause_after_segmented_transport_response(void)
{
    struct timespec delay;

    delay.tv_sec = 0;
    delay.tv_nsec = 2L * 1000L * 1000L;
    while (nanosleep(&delay, &delay) != 0 && errno == EINTR) {
    }
}

static int send_transport_frame_segmented(int fd, const uint8_t* frame, size_t frame_length, const char* log_label)
{
    enum { native_wire_cotp_user_data_segment_length = 1021U };
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsTransportFrame decoded_frame;
    size_t consumed_length = 0U;
    size_t offset = 0U;
    size_t segment_count = 0U;

    if (frame == NULL || frame_length == 0U) {
        return 0;
    }
    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_transport_frame_init(&decoded_frame);
    if (!unitlab_mms_transport_frame_decode(&decoded_frame, frame, frame_length, &consumed_length, &diagnostic)
        || decoded_frame.cotp.kind != UNITLAB_MMS_COTP_TPDU_DT
        || decoded_frame.cotp.user_data == NULL
        || decoded_frame.cotp.user_data_length <= native_wire_cotp_user_data_segment_length) {
        return send_all(fd, frame, frame_length);
    }

    while (offset < decoded_frame.cotp.user_data_length) {
        UnitLabMmsTransportFrame segment_frame;
        uint8_t segment_bytes[1200U];
        size_t remaining_length = decoded_frame.cotp.user_data_length - offset;
        size_t chunk_length = remaining_length > native_wire_cotp_user_data_segment_length ? native_wire_cotp_user_data_segment_length : remaining_length;
        size_t segment_length = 0U;

        unitlab_mms_transport_frame_init(&segment_frame);
        segment_frame.cotp.kind = UNITLAB_MMS_COTP_TPDU_DT;
        segment_frame.cotp.eot = (offset + chunk_length) >= decoded_frame.cotp.user_data_length;
        segment_frame.cotp.user_data = &decoded_frame.cotp.user_data[offset];
        segment_frame.cotp.user_data_length = chunk_length;
        if (!unitlab_mms_transport_frame_encode(&segment_frame, segment_bytes, sizeof(segment_bytes), &segment_length, &diagnostic)) {
            return 0;
        }
        if (!send_all(fd, segment_bytes, segment_length)) {
            return 0;
        }
        offset += chunk_length;
        segment_count++;
    }
    printf(
        "native-wire-server: cotp-segmented-send label=%s original-bytes=%zu user-data=%zu segments=%zu chunk=%u\n",
        log_label != NULL && log_label[0] != '\0' ? log_label : "response",
        frame_length,
        decoded_frame.cotp.user_data_length,
        segment_count,
        (unsigned)native_wire_cotp_user_data_segment_length);
    fflush(stdout);
    pause_after_segmented_transport_response();
    return 1;
}

static int send_pending_information_report(
    UnitLabMmsServerRuntime* server_runtime,
    int data_client_fd,
    const char* log_label,
    UnitLabIedModelLoadResult* result)
{
    UnitLabMmsDiagnostic diagnostic;
    uint8_t response_frame[65535U];
    size_t response_length = 0U;

    if (server_runtime == NULL || data_client_fd < 0 || !unitlab_mms_server_runtime_has_pending_gi_report(server_runtime)) {
        return 1;
    }
    unitlab_mms_diagnostic_clear(&diagnostic);
    if (!unitlab_mms_server_runtime_build_pending_gi_report_bytes(
            server_runtime,
            response_frame,
            sizeof(response_frame),
            &response_length,
            &diagnostic)) {
        set_result(result, "NATIVE_WIRE_SERVER_REPORT_BUILD_FAILED", diagnostic.message);
        return 0;
    }
    if (!send_transport_frame_segmented(data_client_fd, response_frame, response_length, log_label)) {
        set_result(result, "NATIVE_WIRE_SERVER_REPORT_SEND_FAILED", "Native wire server could not send information report frame.");
        return 0;
    }
    printf("native-wire-server: %s bytes=%zu\n", log_label != NULL ? log_label : "information-report-sent", response_length);
    fflush(stdout);
    return 1;
}

static const char* native_wire_test_tick_reference(const UnitLabMmsServerRuntime* server_runtime)
{
    const UnitLabIedModelReportControl* report;
    const UnitLabIedModelDataSet* data_set;

    if (server_runtime == NULL || server_runtime->model_plan == NULL || server_runtime->model_plan->report_count == 0U || server_runtime->model_plan->reports == NULL) {
        return NULL;
    }
    report = server_runtime_active_model_report_control(server_runtime);
    if (report->data_set_index >= server_runtime->model_plan->data_set_count || server_runtime->model_plan->data_sets == NULL || server_runtime->model_plan->signals == NULL) {
        return NULL;
    }
    data_set = &server_runtime->model_plan->data_sets[report->data_set_index];
    for (size_t index = 0U; index < data_set->member_count; index++) {
        size_t signal_index = data_set->first_signal_index + index;
        const UnitLabIedModelSignal* signal;
        if (signal_index >= server_runtime->model_plan->signal_count) {
            break;
        }
        signal = &server_runtime->model_plan->signals[signal_index];
        if (strstr(signal->data_set_entry_variable, "PGGIO1$ST$Ind1$stVal") != NULL) {
            return signal->data_set_entry_variable;
        }
    }
    if (data_set->member_count > 1U && data_set->first_signal_index + 1U < server_runtime->model_plan->signal_count) {
        return server_runtime->model_plan->signals[data_set->first_signal_index + 1U].data_set_entry_variable;
    }
    return NULL;
}

static int native_wire_emit_integrity_if_due(
    UnitLabMmsServerRuntime* server_runtime,
    int data_client_fd,
    UnitLabIedModelLoadResult* result)
{
    UnitLabMmsDiagnostic diagnostic;

    if (server_runtime == NULL || data_client_fd < 0 || unitlab_mms_server_runtime_has_pending_gi_report(server_runtime)) {
        return 1;
    }
    unitlab_mms_diagnostic_clear(&diagnostic);
    if (!server_runtime_poll_integrity_report(server_runtime, native_wire_now_ms(), &diagnostic)) {
        set_result(result, "NATIVE_WIRE_SERVER_INTEGRITY_POLL_FAILED", diagnostic.message);
        return 0;
    }
    return send_pending_information_report(server_runtime, data_client_fd, "integrity-report-sent", result);
}

static int native_wire_emit_test_tick_if_due(
    UnitLabMmsServerRuntime* server_runtime,
    int data_client_fd,
    int interval_ms,
    uint64_t* next_tick_ms,
    uint8_t* tick_value,
    UnitLabIedModelLoadResult* result)
{
    uint64_t now_ms;
    const char* object_reference;
    UnitLabMmsDiagnostic diagnostic;

    if (interval_ms <= 0 || server_runtime == NULL || next_tick_ms == NULL || tick_value == NULL || data_client_fd < 0) {
        return 1;
    }
    if (server_runtime->brcb_rpt_ena == 0U || unitlab_mms_server_runtime_has_pending_gi_report(server_runtime)) {
        return 1;
    }
    now_ms = native_wire_now_ms();
    if (*next_tick_ms == 0U) {
        *next_tick_ms = now_ms + (uint64_t)interval_ms;
        return 1;
    }
    if (now_ms < *next_tick_ms) {
        return 1;
    }
    object_reference = native_wire_test_tick_reference(server_runtime);
    if (object_reference == NULL) {
        set_result(result, "NATIVE_WIRE_SERVER_TEST_TICK_REFERENCE_MISSING", "Native test report tick could not find a report DataSet member to toggle.");
        return 0;
    }
    *tick_value = *tick_value == 0U ? 1U : 0U;
    unitlab_mms_diagnostic_clear(&diagnostic);
    if (!unitlab_mms_server_runtime_update_signal_int32(server_runtime, object_reference, (int32_t)*tick_value, &diagnostic)) {
        set_result(result, "NATIVE_WIRE_SERVER_TEST_TICK_UPDATE_FAILED", diagnostic.message);
        return 0;
    }
    if (!unitlab_mms_server_runtime_has_pending_gi_report(server_runtime)) {
        printf(
            "native-wire-server: test-report-tick-suppressed object=%s value=%u interval-ms=%d reason=no-trigger\n",
            object_reference,
            (unsigned)*tick_value,
            interval_ms);
        fflush(stdout);
        *next_tick_ms = now_ms + (uint64_t)interval_ms;
        return 1;
    }
    printf(
        "native-wire-server: test-report-tick object=%s value=%u interval-ms=%d\n",
        object_reference,
        (unsigned)*tick_value,
        interval_ms);
    fflush(stdout);
    *next_tick_ms = now_ms + (uint64_t)interval_ms;
    return send_pending_information_report(server_runtime, data_client_fd, "test-data-change-report-sent", result);
}

static int emit_text_response(int response_fd, const char* text)
{
    size_t length = strlen(text);
    if (response_fd >= 0) {
        return send_all(response_fd, (const uint8_t*)text, length) && send_all(response_fd, (const uint8_t*)"\n", 1U);
    }
    printf("%s\n", text);
    fflush(stdout);
    return 1;
}

static int format_hex_response(const uint8_t* frame, size_t frame_length, char* response, size_t response_length)
{
    static const char hex_digits[] = "0123456789abcdef";
    size_t required_length = 11U + (frame_length * 2U) + 1U;
    size_t offset = 0U;

    if (response == NULL || response_length == 0U) {
        return 0;
    }
    if (response_length < required_length) {
        return 0;
    }
    memcpy(response, "wire-frame=", 11U);
    offset = 11U;
    for (size_t index = 0U; index < frame_length; index++) {
        response[offset++] = hex_digits[(frame[index] >> 4) & 0x0FU];
        response[offset++] = hex_digits[frame[index] & 0x0FU];
    }
    response[offset] = '\0';
    return 1;
}


static int append_text(char* buffer, size_t buffer_size, size_t* offset, const char* text)
{
    size_t length;
    if (buffer == NULL || offset == NULL || text == NULL) {
        return 0;
    }
    length = strlen(text);
    if (*offset + length >= buffer_size) {
        return 0;
    }
    memcpy(buffer + *offset, text, length);
    *offset += length;
    buffer[*offset] = '\0';
    return 1;
}

static int append_json_string(char* buffer, size_t buffer_size, size_t* offset, const char* value)
{
    const unsigned char* current;

    if (!append_text(buffer, buffer_size, offset, "\"")) {
        return 0;
    }
    current = (const unsigned char*)(value != NULL ? value : "");
    while (*current != '\0') {
        char escaped[7U];
        if (*current == '\"' || *current == '\\') {
            escaped[0] = '\\';
            escaped[1] = (char)*current;
            escaped[2] = '\0';
            if (!append_text(buffer, buffer_size, offset, escaped)) {
                return 0;
            }
        } else if (*current < 0x20U) {
            snprintf(escaped, sizeof(escaped), "\\u%04x", (unsigned)*current);
            if (!append_text(buffer, buffer_size, offset, escaped)) {
                return 0;
            }
        } else {
            if (*offset + 1U >= buffer_size) {
                return 0;
            }
            buffer[(*offset)++] = (char)*current;
            buffer[*offset] = '\0';
        }
        current++;
    }
    return append_text(buffer, buffer_size, offset, "\"");
}

static int hex_nibble_value(char value)
{
    if (value >= '0' && value <= '9') {
        return value - '0';
    }
    if (value >= 'a' && value <= 'f') {
        return value - 'a' + 10;
    }
    if (value >= 'A' && value <= 'F') {
        return value - 'A' + 10;
    }
    return -1;
}

static int decode_hex_argument(const char* hex, char* output, size_t output_size)
{
    size_t length;
    size_t output_length;

    if (hex == NULL || output == NULL || output_size == 0U) {
        return 0;
    }
    length = strlen(hex);
    if ((length % 2U) != 0U) {
        return 0;
    }
    output_length = length / 2U;
    if (output_length >= output_size) {
        return 0;
    }
    for (size_t index = 0U; index < output_length; index++) {
        int high = hex_nibble_value(hex[index * 2U]);
        int low = hex_nibble_value(hex[index * 2U + 1U]);
        if (high < 0 || low < 0) {
            return 0;
        }
        output[index] = (char)((high << 4) | low);
    }
    output[output_length] = '\0';
    return 1;
}

static int parse_boolean_argument(const char* value, int* parsed)
{
    if (value == NULL || parsed == NULL) {
        return 0;
    }
    if (strcmp(value, "true") == 0 || strcmp(value, "1") == 0 || strcmp(value, "on") == 0) {
        *parsed = 1;
        return 1;
    }
    if (strcmp(value, "false") == 0 || strcmp(value, "0") == 0 || strcmp(value, "off") == 0) {
        *parsed = 0;
        return 1;
    }
    return 0;
}

static int parse_int32_argument(const char* value, int32_t* parsed)
{
    char* end = NULL;
    long result;

    if (value == NULL || parsed == NULL || value[0] == '\0') {
        return 0;
    }
    errno = 0;
    result = strtol(value, &end, 10);
    if (errno != 0 || end == value || end == NULL || *end != '\0' || result < INT32_MIN || result > INT32_MAX) {
        return 0;
    }
    *parsed = (int32_t)result;
    return 1;
}

static int encode_real32_argument(const char* value, uint8_t* encoded, size_t encoded_size, size_t* encoded_length)
{
    char* end = NULL;
    double parsed;
    float real_value;
    uint32_t real_bits = 0U;

    if (value == NULL || encoded == NULL || encoded_size < 5U || encoded_length == NULL || value[0] == '\0') {
        return 0;
    }
    errno = 0;
    parsed = strtod(value, &end);
    if (errno != 0 || end == value || end == NULL || *end != '\0' || !isfinite(parsed)) {
        return 0;
    }
    real_value = (float)parsed;
    memcpy(&real_bits, &real_value, sizeof(real_bits));
    encoded[0] = 0x08U;
    encoded[1] = (uint8_t)((real_bits >> 24U) & 0xFFU);
    encoded[2] = (uint8_t)((real_bits >> 16U) & 0xFFU);
    encoded[3] = (uint8_t)((real_bits >> 8U) & 0xFFU);
    encoded[4] = (uint8_t)(real_bits & 0xFFU);
    *encoded_length = 5U;
    return 1;
}

static const char* pending_report_kind_label(UnitLabMmsServerPendingReportKind kind)
{
    switch (kind) {
        case UNITLAB_MMS_SERVER_PENDING_REPORT_GI:
            return "gi";
        case UNITLAB_MMS_SERVER_PENDING_REPORT_DATA_CHANGE:
            return "data-change";
        case UNITLAB_MMS_SERVER_PENDING_REPORT_QUALITY_CHANGE:
            return "quality-change";
        case UNITLAB_MMS_SERVER_PENDING_REPORT_DATA_UPDATE:
            return "data-update";
        case UNITLAB_MMS_SERVER_PENDING_REPORT_INTEGRITY:
            return "integrity";
        case UNITLAB_MMS_SERVER_PENDING_REPORT_NONE:
        default:
            return "none";
    }
}

static void socket_peer_label(int fd, char* output, size_t output_size)
{
    struct sockaddr_storage address;
    socklen_t address_length = sizeof(address);
    char host[128U];
    char service[32U];

    if (output == NULL || output_size == 0U) {
        return;
    }
    output[0] = '\0';
    if (fd < 0) {
        return;
    }
    if (getpeername(fd, (struct sockaddr*)&address, &address_length) != 0) {
        return;
    }
    if (getnameinfo((struct sockaddr*)&address, address_length, host, sizeof(host), service, sizeof(service), NI_NUMERICHOST | NI_NUMERICSERV) != 0) {
        return;
    }
    snprintf(output, output_size, "%s:%s", host, service);
}

static int emit_control_error(int response_fd, const char* code, const char* message)
{
    char response[512U];
    size_t offset = 0U;

    response[0] = '\0';
    if (!append_text(response, sizeof(response), &offset, "{\"ok\":false,\"code\":")) {
        return 0;
    }
    if (!append_json_string(response, sizeof(response), &offset, code != NULL ? code : "NATIVE_WIRE_CONTROL_ERROR")) {
        return 0;
    }
    if (!append_text(response, sizeof(response), &offset, ",\"message\":")) {
        return 0;
    }
    if (!append_json_string(response, sizeof(response), &offset, message != NULL ? message : "Native wire control command failed.")) {
        return 0;
    }
    if (!append_text(response, sizeof(response), &offset, "}")) {
        return 0;
    }
    return emit_text_response(response_fd, response);
}

static int emit_runtime_status_response(UnitLabMmsServerRuntime* server_runtime, int data_client_fd, int response_fd)
{
    char response[2048U];
    char peer[128U];
    char report_id_reference[160U];
    char data_set_reference[160U];
    const UnitLabIedModelReportControl* report = NULL;
    size_t offset = 0U;
    char number_text[64U];

    if (server_runtime == NULL) {
        return emit_control_error(response_fd, "NATIVE_WIRE_RUNTIME_MISSING", "Native wire runtime is not available.");
    }
    response[0] = '\0';
    peer[0] = '\0';
    report_id_reference[0] = '\0';
    data_set_reference[0] = '\0';
    socket_peer_label(data_client_fd, peer, sizeof(peer));
    report = server_runtime_active_model_report_control(server_runtime);
    server_runtime_format_report_control_references(server_runtime, report_id_reference, sizeof(report_id_reference), data_set_reference, sizeof(data_set_reference));

    if (!append_text(response, sizeof(response), &offset, "{\"ok\":true")) return 0;
    if (!append_text(response, sizeof(response), &offset, ",\"dataClientConnected\":")) return 0;
    if (!append_text(response, sizeof(response), &offset, data_client_fd >= 0 ? "true" : "false")) return 0;
    if (!append_text(response, sizeof(response), &offset, ",\"dataClient\":")) return 0;
    if (!append_json_string(response, sizeof(response), &offset, peer)) return 0;
    if (!append_text(response, sizeof(response), &offset, ",\"reportEnabled\":")) return 0;
    if (!append_text(response, sizeof(response), &offset, server_runtime->brcb_rpt_ena != 0U ? "true" : "false")) return 0;
    if (!append_text(response, sizeof(response), &offset, ",\"activeReport\":")) return 0;
    if (!append_json_string(response, sizeof(response), &offset, report != NULL ? report->name : "")) return 0;
    if (!append_text(response, sizeof(response), &offset, ",\"activeReportKey\":")) return 0;
    if (!append_json_string(response, sizeof(response), &offset, report != NULL ? report->key : "")) return 0;
    if (!append_text(response, sizeof(response), &offset, ",\"reportKind\":")) return 0;
    if (!append_json_string(response, sizeof(response), &offset, report != NULL ? report->report_kind : "")) return 0;
    if (!append_text(response, sizeof(response), &offset, ",\"reportIdReference\":")) return 0;
    if (!append_json_string(response, sizeof(response), &offset, report_id_reference)) return 0;
    if (!append_text(response, sizeof(response), &offset, ",\"dataSetRef\":")) return 0;
    if (!append_json_string(response, sizeof(response), &offset, report != NULL ? report->data_set_ref : "")) return 0;
    if (!append_text(response, sizeof(response), &offset, ",\"dataSetReference\":")) return 0;
    if (!append_json_string(response, sizeof(response), &offset, data_set_reference)) return 0;
    if (!append_text(response, sizeof(response), &offset, ",\"owner\":")) return 0;
    if (!append_json_string(response, sizeof(response), &offset, server_runtime->brcb_owner)) return 0;
    if (!append_text(response, sizeof(response), &offset, ",\"pendingReportKind\":")) return 0;
    if (!append_json_string(response, sizeof(response), &offset, pending_report_kind_label(server_runtime->pending_report_kind))) return 0;
    snprintf(number_text, sizeof(number_text), ",\"pendingReportQueueCount\":%zu", server_runtime->pending_report_queue_count);
    if (!append_text(response, sizeof(response), &offset, number_text)) return 0;
    snprintf(number_text, sizeof(number_text), ",\"reportsSent\":%llu", (unsigned long long)server_runtime->reports_sent);
    if (!append_text(response, sizeof(response), &offset, number_text)) return 0;
    snprintf(number_text, sizeof(number_text), ",\"reportEventsQueued\":%llu", (unsigned long long)server_runtime->report_events_queued);
    if (!append_text(response, sizeof(response), &offset, number_text)) return 0;
    if (!append_text(response, sizeof(response), &offset, "}")) return 0;

    return emit_text_response(response_fd, response);
}

static int handle_update_signal_command(
    UnitLabMmsServerRuntime* server_runtime,
    int data_client_fd,
    int response_fd,
    const char* command,
    UnitLabIedModelLoadResult* result)
{
    char command_copy[1024U];
    char* context = NULL;
    char* token = NULL;
    char* kind = NULL;
    char* object_reference_hex = NULL;
    char* value_hex = NULL;
    char object_reference[256U];
    char value[256U];
    UnitLabMmsDiagnostic diagnostic;
    int update_ok = 0;
    int report_queued = 0;
    int report_sent = 0;
    char response[1024U];
    size_t offset = 0U;

    if (server_runtime == NULL || command == NULL) {
        return emit_control_error(response_fd, "NATIVE_WIRE_UPDATE_SIGNAL_INVALID", "Signal update requires runtime and command.");
    }
    if (strlen(command) >= sizeof(command_copy)) {
        return emit_control_error(response_fd, "NATIVE_WIRE_UPDATE_SIGNAL_TOO_LONG", "Signal update command is too long.");
    }
    memcpy(command_copy, command, strlen(command) + 1U);
    token = strtok_r(command_copy, " ", &context);
    (void)token;
    kind = strtok_r(NULL, " ", &context);
    object_reference_hex = strtok_r(NULL, " ", &context);
    value_hex = strtok_r(NULL, " ", &context);
    if (kind == NULL || object_reference_hex == NULL || value_hex == NULL) {
        return emit_control_error(response_fd, "NATIVE_WIRE_UPDATE_SIGNAL_ARGS_REQUIRED", "Signal update requires kind, object reference hex, and value hex.");
    }
    if (!decode_hex_argument(object_reference_hex, object_reference, sizeof(object_reference)) || object_reference[0] == '\0') {
        return emit_control_error(response_fd, "NATIVE_WIRE_UPDATE_SIGNAL_OBJECT_INVALID", "Signal update object reference hex is invalid.");
    }
    if (!decode_hex_argument(value_hex, value, sizeof(value))) {
        return emit_control_error(response_fd, "NATIVE_WIRE_UPDATE_SIGNAL_VALUE_INVALID", "Signal update value hex is invalid.");
    }

    unitlab_mms_diagnostic_clear(&diagnostic);
    if (strcmp(kind, "boolean") == 0 || strcmp(kind, "bool") == 0) {
        int parsed = 0;
        if (!parse_boolean_argument(value, &parsed)) {
            return emit_control_error(response_fd, "NATIVE_WIRE_UPDATE_SIGNAL_BOOLEAN_INVALID", "Boolean signal update value must be true/false or 1/0.");
        }
        update_ok = unitlab_mms_server_runtime_update_signal_boolean(server_runtime, object_reference, parsed, &diagnostic);
    } else if (strcmp(kind, "integer") == 0 || strcmp(kind, "int32") == 0 || strcmp(kind, "enum") == 0) {
        int32_t parsed = 0;
        if (!parse_int32_argument(value, &parsed)) {
            return emit_control_error(response_fd, "NATIVE_WIRE_UPDATE_SIGNAL_INTEGER_INVALID", "Integer signal update value must fit int32.");
        }
        update_ok = strcmp(kind, "enum") == 0
            ? unitlab_mms_server_runtime_update_signal_enum(server_runtime, object_reference, (int)parsed, &diagnostic)
            : unitlab_mms_server_runtime_update_signal_int32(server_runtime, object_reference, parsed, &diagnostic);
    } else if (strcmp(kind, "real") == 0 || strcmp(kind, "float32") == 0) {
        uint8_t encoded_real[5U];
        size_t encoded_real_length = 0U;
        if (!encode_real32_argument(value, encoded_real, sizeof(encoded_real), &encoded_real_length)) {
            return emit_control_error(response_fd, "NATIVE_WIRE_UPDATE_SIGNAL_REAL_INVALID", "Real signal update value must be a finite number.");
        }
        update_ok = unitlab_mms_server_runtime_update_signal_value(server_runtime, object_reference, encoded_real, encoded_real_length, &diagnostic);
    } else if (strcmp(kind, "string") == 0 || strcmp(kind, "visible-string") == 0) {
        update_ok = unitlab_mms_server_runtime_update_signal_visible_string(server_runtime, object_reference, value, &diagnostic);
    } else {
        return emit_control_error(response_fd, "NATIVE_WIRE_UPDATE_SIGNAL_KIND_INVALID", "Signal update kind is unsupported.");
    }

    if (!update_ok) {
        return emit_control_error(response_fd, "NATIVE_WIRE_UPDATE_SIGNAL_FAILED", diagnostic.message[0] != '\0' ? diagnostic.message : "Signal update failed.");
    }

    report_queued = unitlab_mms_server_runtime_has_pending_gi_report(server_runtime) != 0;
    if (report_queued && data_client_fd >= 0) {
        if (!send_pending_information_report(server_runtime, data_client_fd, "ui-signal-update-report-sent", result)) {
            return 0;
        }
        report_sent = 1;
    }

    response[0] = '\0';
    if (!append_text(response, sizeof(response), &offset, "{\"ok\":true,\"objectReference\":")) return 0;
    if (!append_json_string(response, sizeof(response), &offset, object_reference)) return 0;
    if (!append_text(response, sizeof(response), &offset, ",\"valueKind\":")) return 0;
    if (!append_json_string(response, sizeof(response), &offset, kind)) return 0;
    if (!append_text(response, sizeof(response), &offset, ",\"value\":")) return 0;
    if (!append_json_string(response, sizeof(response), &offset, value)) return 0;
    if (!append_text(response, sizeof(response), &offset, ",\"reportQueued\":")) return 0;
    if (!append_text(response, sizeof(response), &offset, report_queued ? "true" : "false")) return 0;
    if (!append_text(response, sizeof(response), &offset, ",\"reportSent\":")) return 0;
    if (!append_text(response, sizeof(response), &offset, report_sent ? "true" : "false")) return 0;
    if (!append_text(response, sizeof(response), &offset, ",\"pendingReportKind\":")) return 0;
    if (!append_json_string(response, sizeof(response), &offset, pending_report_kind_label(server_runtime->pending_report_kind))) return 0;
    if (!append_text(response, sizeof(response), &offset, ",\"message\":\"signal updated\"}")) return 0;

    printf(
        "native-wire-server: ui-signal-update object=%s kind=%s value=%s report-queued=%d report-sent=%d\n",
        object_reference,
        kind,
        value,
        report_queued,
        report_sent);
    fflush(stdout);
    return emit_text_response(response_fd, response);
}

static void log_hex_bytes(const char* label, const uint8_t* bytes, size_t length)
{
    if (label == NULL) {
        return;
    }
    printf("native-wire-server: %s=", label);
    if (bytes == NULL || length == 0U) {
        printf("<empty>\n");
        fflush(stdout);
        return;
    }
    for (size_t index = 0U; index < length; index++) {
        printf("%02x", bytes[index]);
    }
    printf("\n");
    fflush(stdout);
}

static const char* get_name_list_scope_label(uint32_t browse_object_scope)
{
    switch (browse_object_scope) {
        case 0U:
            return "VMD-SPECIFIC";
        case 1U:
            return "DOMAIN-SPECIFIC";
        case 2U:
            return "AA-SPECIFIC";
        default:
            return "UNKNOWN";
    }
}

static void log_get_name_list_context(const char* prefix, const UnitLabMmsPendingRequest* request)
{
    if (prefix == NULL || request == NULL) {
        return;
    }
    printf(
        "native-wire-server: %s GetNameList(%s) invoke=%u browse-class=%u browse-scope=%u domain=%s continue-after=%s\n",
        prefix,
        get_name_list_scope_label(request->browse_object_scope),
        (unsigned)request->invoke_id,
        (unsigned)request->browse_object_class,
        (unsigned)request->browse_object_scope,
        request->browse_domain_id[0] != '\0' ? request->browse_domain_id : "<none>",
        request->browse_continue_after[0] != '\0' ? request->browse_continue_after : "<none>");
    fflush(stdout);
}

static void log_get_variable_access_attributes_context(const char* prefix, const UnitLabMmsPendingRequest* request)
{
    if (prefix == NULL || request == NULL) {
        return;
    }
    printf(
        "native-wire-server: %s GetVariableAccessAttributes invoke=%u object=%s attribute=%s\n",
        prefix,
        (unsigned)request->invoke_id,
        request->object_reference[0] != '\0' ? request->object_reference : "<none>",
        request->attribute_reference[0] != '\0' ? request->attribute_reference : "<none>");
    fflush(stdout);
}

static void log_unsupported_mms_request(UnitLabMmsServerRuntime* server_runtime, const uint8_t* incoming, size_t received, const UnitLabMmsOperationResult* incoming_result)
{
    UnitLabMmsSemanticResult semantic_result;
    UnitLabMmsDecodeDiagnostic bridge_diagnostic;
    const UnitLabMmsPdu* wire_pdu = NULL;

    if (server_runtime == NULL || incoming_result == NULL) {
        return;
    }
    wire_pdu = &server_runtime->last_wire_pdu;
    unitlab_mms_semantic_result_init(&semantic_result);
    unitlab_mms_decode_diagnostic_init(&bridge_diagnostic);
    if (unitlab_mms_semantic_result_from_wire_pdu(&semantic_result, wire_pdu, &bridge_diagnostic)) {
        if (wire_pdu->service_kind == UNITLAB_MMS_SERVICE_GET_VARIABLE_ACCESS_ATTRIBUTES) {
            printf(
                "native-wire-server: unsupported-mms-request invoke=%u service-kind=%u service-tag=%u/%u constructed=%d object-reference=%s attribute=%s\n",
                (unsigned)(semantic_result.pdu.invoke_id),
                (unsigned)wire_pdu->service_kind,
                (unsigned)wire_pdu->service_tag.tag_class,
                (unsigned)wire_pdu->service_tag.tag_number,
                wire_pdu->service_tag.constructed,
                semantic_result.pdu.object_reference[0] != '\0' ? semantic_result.pdu.object_reference : "<none>",
                semantic_result.pdu.attribute_reference[0] != '\0' ? semantic_result.pdu.attribute_reference : "<none>");
        }
        else {
            printf(
                "native-wire-server: unsupported-mms-request invoke=%u service-kind=%u service-tag=%u/%u constructed=%d object-class=%u scope=%u domain=%s continue-after=%s\n",
                (unsigned)(semantic_result.pdu.invoke_id),
                (unsigned)wire_pdu->service_kind,
                (unsigned)wire_pdu->service_tag.tag_class,
                (unsigned)wire_pdu->service_tag.tag_number,
                wire_pdu->service_tag.constructed,
                (unsigned)semantic_result.pdu.object_class,
                (unsigned)semantic_result.pdu.object_scope,
                semantic_result.pdu.domain_id[0] != '\0' ? semantic_result.pdu.domain_id : "<none>",
                semantic_result.pdu.continue_after[0] != '\0' ? semantic_result.pdu.continue_after : "<none>");
        }
    }
    else {
        printf(
            "native-wire-server: unsupported-mms-request invoke=%u service-kind=%u service-tag=%u/%u constructed=%d diag=%d %s\n",
            (unsigned)(wire_pdu != NULL && wire_pdu->has_invoke_id ? wire_pdu->invoke_id : 0U),
            (unsigned)(wire_pdu != NULL ? wire_pdu->service_kind : 0U),
            (unsigned)(wire_pdu != NULL ? wire_pdu->service_tag.tag_class : 0U),
            (unsigned)(wire_pdu != NULL ? wire_pdu->service_tag.tag_number : 0U),
            wire_pdu != NULL ? wire_pdu->service_tag.constructed : 0,
            (int)bridge_diagnostic.diagnostic.code,
            bridge_diagnostic.diagnostic.message);
    }
    log_hex_bytes("incoming-wire-hex", incoming, received);
    if (wire_pdu != NULL) {
        log_hex_bytes("mms-pdu-hex", wire_pdu->pdu_bytes, wire_pdu->pdu_length);
    }
    printf(
        "native-wire-server: unsupported-result code=%d message=%s pending-state=%u pending-kind=%u last-event=%u\n",
        (int)incoming_result->diagnostic.code,
        incoming_result->diagnostic.message,
        (unsigned)server_runtime->pending_request.state,
        (unsigned)server_runtime->pending_request.kind,
        (unsigned)server_runtime->pending_request.last_event.kind);
    fflush(stdout);
}

static uint64_t native_wire_now_ms(void)
{
    return (uint64_t)time(NULL) * 1000ULL;
}
static int build_native_association_response_frame(
    UnitLabMmsServerRuntime* server_runtime,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    return unitlab_mms_build_association_response_frame_with_profile(&server_runtime->initiate_response_profile, buffer, buffer_length, encoded_length, diagnostic);
}
static int build_native_information_report_frame(
    UnitLabMmsServerRuntime* server_runtime,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    uint8_t scratch[256U];
    (void)server_runtime;
    return unitlab_mms_build_information_report_frame("RPT", 0U, scratch, sizeof(scratch), buffer, buffer_length, encoded_length, diagnostic);
}

static void reset_native_wire_runtime_state(UnitLabMmsServerRuntime* server_runtime)
{
    if (server_runtime == NULL) {
        return;
    }
    unitlab_mms_session_init(&server_runtime->session);
    unitlab_mms_pending_request_init(&server_runtime->pending_request);
    unitlab_mms_transport_exchange_init(&server_runtime->transport);
    unitlab_mms_pdu_init(&server_runtime->last_wire_pdu);
    server_runtime->last_incoming_invoke_id = 0U;
    server_runtime->last_incoming_service[0] = '\0';
    server_runtime->last_outgoing_invoke_id = 0U;
    server_runtime->last_outgoing_service[0] = '\0';
    server_runtime->last_outgoing_summary[0] = '\0';
    unitlab_mms_server_runtime_capture_snapshot(server_runtime);
    printf("native-wire-server: runtime-reset session-state=%u pending-state=%u transport-invoke=%u\n", (unsigned)server_runtime->session.state, (unsigned)server_runtime->pending_request.state, (unsigned)server_runtime->transport.invoke_id);
    fflush(stdout);
}
static int native_wire_process_received_tpkt_frame(
    UnitLabMmsServerRuntime* server_runtime,
    int data_client_fd,
    const uint8_t* incoming,
    size_t received,
    UnitLabIedModelLoadResult* result)
{
    UnitLabMmsOperationResult incoming_result;
    UnitLabMmsDiagnostic response_diagnostic;
    uint8_t response_frame[65535U];
    size_t consumed_length = 0U;
    size_t response_length = 0U;
    UnitLabMmsTransportFrame incoming_transport;
    int apply_ok;

    if (server_runtime == NULL || incoming == NULL || received == 0U || result == NULL) {
        return -1;
    }
    unitlab_mms_operation_result_init(&incoming_result);
    unitlab_mms_diagnostic_clear(&response_diagnostic);
    printf("native-wire-server: pre-association session-state=%u incoming-bytes=%zu\n", (unsigned)server_runtime->session.state, received);
    fflush(stdout);
    unitlab_mms_transport_frame_init(&incoming_transport);
    if (unitlab_mms_transport_frame_decode(&incoming_transport, incoming, received, &consumed_length, &response_diagnostic)
        && incoming_transport.cotp.kind == UNITLAB_MMS_COTP_TPDU_CR) {
        if (!unitlab_mms_build_cotp_connect_response_frame(incoming_transport.cotp.user_data, incoming_transport.cotp.user_data_length, response_frame, sizeof(response_frame), &response_length, &response_diagnostic)) {
            set_result(result, "NATIVE_WIRE_SERVER_COTP_CC_BUILD_FAILED", response_diagnostic.message);
            return -1;
        }
        printf("native-wire-server: received-cotp-cr bytes=%zu\n", received);
        fflush(stdout);
        if (!send_all(data_client_fd, response_frame, response_length)) {
            set_result(result, "NATIVE_WIRE_SERVER_COTP_CC_SEND_FAILED", "Native wire server could not send COTP connect response frame.");
            return -1;
        }
        printf("native-wire-server: received-cotp-cr bytes=%zu\n", received);
        printf("native-wire-server: sent-cotp-cc bytes=%zu\n", response_length);
        fflush(stdout);
        return 1;
    }
    unitlab_mms_diagnostic_clear(&response_diagnostic);
    if (server_runtime->session.state != UNITLAB_MMS_SESSION_ASSOCIATED
        && unitlab_mms_server_runtime_apply_association_request_bytes(server_runtime, incoming, received, &consumed_length, &incoming_result)) {
        if (!build_native_association_response_frame(server_runtime, response_frame, sizeof(response_frame), &response_length, &response_diagnostic)) {
            set_result(result, "NATIVE_WIRE_SERVER_ASSOCIATION_RESPONSE_BUILD_FAILED", response_diagnostic.message);
            return -1;
        }
        if (!send_all(data_client_fd, response_frame, response_length)) {
            set_result(result, "NATIVE_WIRE_SERVER_ASSOCIATION_RESPONSE_SEND_FAILED", "Native wire server could not send association response frame.");
            return -1;
        }
        if (!unitlab_mms_session_complete_association(&server_runtime->session, server_runtime->session.active_invoke_id, &response_diagnostic)) {
            set_result(result, "NATIVE_WIRE_SERVER_ASSOCIATION_COMPLETE_FAILED", response_diagnostic.message);
            return -1;
        }
        printf("native-wire-server: association-response-sent bytes=%zu\n", response_length);
        fflush(stdout);
        return 1;
    }
    if (incoming_result.diagnostic.code != UNITLAB_MMS_DIAGNOSTIC_OK) {
        printf(
            "native-wire-server: association-request-rejected code=%d message=%s\n",
            (int)incoming_result.diagnostic.code,
            incoming_result.diagnostic.message);
        fflush(stdout);
        return 0;
    }
    apply_ok = unitlab_mms_server_runtime_apply_incoming_bytes(server_runtime, incoming, received, &consumed_length, &incoming_result);
    if (apply_ok
        && server_runtime->last_wire_pdu.kind == UNITLAB_MMS_PDU_CONCLUDE_REQUEST
        && server_runtime->session.state == UNITLAB_MMS_SESSION_RELEASING) {
        if (!unitlab_mms_server_runtime_build_release_response_bytes(
                server_runtime,
                response_frame,
                sizeof(response_frame),
                &response_length,
                &response_diagnostic)) {
            set_result(result, "NATIVE_WIRE_SERVER_RELEASE_RESPONSE_BUILD_FAILED", response_diagnostic.message);
            return -1;
        }
        if (!send_all(data_client_fd, response_frame, response_length)) {
            set_result(result, "NATIVE_WIRE_SERVER_RELEASE_RESPONSE_SEND_FAILED", "Native wire server could not send MMS release response frame.");
            return -1;
        }
        if (!unitlab_mms_session_complete_release(&server_runtime->session, &response_diagnostic)) {
            set_result(result, "NATIVE_WIRE_SERVER_RELEASE_COMPLETE_FAILED", response_diagnostic.message);
            return -1;
        }
        printf("native-wire-server: release-response-sent bytes=%zu closing-data-socket=true\n", response_length);
        fflush(stdout);
        return 2;
    }
    if (apply_ok
        && server_runtime->pending_request.state == UNITLAB_MMS_PENDING_REQUEST_ACTIVE) {
        if (!unitlab_mms_server_runtime_build_confirmed_response_bytes(
                server_runtime,
                NULL,
                0U,
                response_frame,
                sizeof(response_frame),
                &response_length,
                &response_diagnostic)) {
            if (server_runtime->pending_request.kind == UNITLAB_MMS_REQUEST_GET_NAME_LIST) {
                log_get_name_list_context("response-build-failed", &server_runtime->pending_request);
            } else if (server_runtime->pending_request.kind == UNITLAB_MMS_REQUEST_GET_VARIABLE_ACCESS_ATTRIBUTES) {
                log_get_variable_access_attributes_context("response-build-failed", &server_runtime->pending_request);
            }
            printf(
                "native-wire-server: response-build-failed code=%d message=%s pending-state=%u pending-kind=%u invoke=%u browse-class=%u browse-scope=%u domain=%s continue-after=%s consumed=%zu\n",
                (int)response_diagnostic.code,
                response_diagnostic.message,
                (unsigned)server_runtime->pending_request.state,
                (unsigned)server_runtime->pending_request.kind,
                (unsigned)server_runtime->pending_request.invoke_id,
                (unsigned)server_runtime->pending_request.browse_object_class,
                (unsigned)server_runtime->pending_request.browse_object_scope,
                server_runtime->pending_request.browse_domain_id[0] != '\0' ? server_runtime->pending_request.browse_domain_id : "<none>",
                server_runtime->pending_request.browse_continue_after[0] != '\0' ? server_runtime->pending_request.browse_continue_after : "<none>",
                consumed_length);
            fflush(stdout);
            set_result(result, "NATIVE_WIRE_SERVER_RESPONSE_BUILD_FAILED", response_diagnostic.message);
            return -1;
        }
        if (!send_transport_frame_segmented(data_client_fd, response_frame, response_length, "confirmed-response")) {
            set_result(result, "NATIVE_WIRE_SERVER_RESPONSE_SEND_FAILED", "Native wire server could not send confirmed response frame.");
            return -1;
        }
        if (!unitlab_mms_pending_request_complete(&server_runtime->pending_request, native_wire_now_ms(), &response_diagnostic)) {
            set_result(result, "NATIVE_WIRE_SERVER_REQUEST_COMPLETE_FAILED", response_diagnostic.message);
            return -1;
        }
        printf("native-wire-server: confirmed-response-sent bytes=%zu\n", response_length);
        fflush(stdout);
        if (!send_pending_information_report(server_runtime, data_client_fd, "gi-information-report-sent", result)) {
            return -1;
        }
    }
    else {
        if (incoming_result.diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED
            && server_runtime->last_wire_pdu.kind == UNITLAB_MMS_PDU_CONFIRMED_REQUEST
            && server_runtime->last_wire_pdu.has_invoke_id) {
            if (server_runtime->pending_request.kind == UNITLAB_MMS_REQUEST_GET_NAME_LIST) {
                log_get_name_list_context("unsupported", &server_runtime->pending_request);
            } else if (server_runtime->pending_request.kind == UNITLAB_MMS_REQUEST_GET_VARIABLE_ACCESS_ATTRIBUTES) {
                log_get_variable_access_attributes_context("unsupported", &server_runtime->pending_request);
            }
            log_unsupported_mms_request(server_runtime, incoming, received, &incoming_result);
            if (unitlab_mms_server_runtime_build_confirmed_error_bytes(
                    server_runtime,
                    server_runtime->last_wire_pdu.invoke_id,
                    response_frame,
                    sizeof(response_frame),
                    &response_length,
                    &response_diagnostic)) {
                if (send_transport_frame_segmented(data_client_fd, response_frame, response_length, "confirmed-error")) {
                    printf("native-wire-server: confirmed-error-sent invoke=%u bytes=%zu\n", (unsigned)server_runtime->last_wire_pdu.invoke_id, response_length);
                    fflush(stdout);
                } else {
                    set_result(result, "NATIVE_WIRE_SERVER_RESPONSE_SEND_FAILED", "Native wire server could not send confirmed error frame.");
                    return -1;
                }
            } else {
                printf(
                    "native-wire-server: confirmed-error-build-failed code=%d message=%s invoke=%u\n",
                    (int)response_diagnostic.code,
                    response_diagnostic.message,
                    (unsigned)server_runtime->last_wire_pdu.invoke_id);
                fflush(stdout);
            }
        }
        printf(
            "native-wire-server: received-bytes=%zu apply-ok=%d pending-state=%u pending-kind=%u last-event=%u diag=%d %s\n",
            received,
            apply_ok,
            (unsigned)server_runtime->pending_request.state,
            (unsigned)server_runtime->pending_request.kind,
            (unsigned)server_runtime->pending_request.last_event.kind,
            (int)incoming_result.diagnostic.code,
            incoming_result.diagnostic.message);
        fflush(stdout);
    }
    return 1;
}

static int resolve_listener(const char* bind_address, int port, struct addrinfo** out_info)
{
    struct addrinfo hints;
    char port_text[16U];
    int status;
    if (out_info == NULL) {
        return 0;
    }
    *out_info = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    snprintf(port_text, sizeof(port_text), "%d", port);
    status = getaddrinfo(bind_address, port_text, &hints, out_info);
    return status == 0;
}
static int bind_listen(const char* bind_address, int port)
{
    struct addrinfo* listener_info = NULL;
    int listen_fd = -1;
    if (!resolve_listener(bind_address, port, &listener_info)) {
        return -1;
    }
    for (struct addrinfo* current = listener_info; current != NULL; current = current->ai_next) {
        listen_fd = socket(current->ai_family, current->ai_socktype, current->ai_protocol);
        if (listen_fd < 0) {
            continue;
        }
        int yes = 1;
        (void)setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
        if (bind(listen_fd, current->ai_addr, current->ai_addrlen) == 0 && listen(listen_fd, 1) == 0) {
            break;
        }
        close(listen_fd);
        listen_fd = -1;
    }
    freeaddrinfo(listener_info);
    return listen_fd;
}
static int accept_connection(int listen_fd)
{
    int client_fd = accept(listen_fd, NULL, NULL);
    int tcp_no_delay = 1;

    if (client_fd < 0) {
        if (errno == EINTR) {
            return -2;
        }
        return -1;
    }
    (void)setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &tcp_no_delay, sizeof(tcp_no_delay));
    return client_fd;
}
static int read_command_from_socket(int fd, char* command, size_t command_length)
{
    ssize_t received = recv(fd, command, command_length - 1U, 0);
    if (received <= 0) {
        return 0;
    }
    command[received] = '\0';
    char* newline = strchr(command, '\n');
    if (newline != NULL) {
        *newline = '\0';
    }
    return 1;
}
static int handle_command(
    UnitLabMmsServerRuntime* server_runtime,
    int* data_client_fd,
    int data_listen_fd,
    int response_fd,
    const char* command,
    uint8_t* frame,
    size_t frame_length,
    size_t* encoded_length,
    UnitLabIedModelLoadResult* result)
{
    if (strncmp(command, "status", 6U) == 0) {
        if (!emit_runtime_status_response(server_runtime, data_client_fd != NULL ? *data_client_fd : -1, response_fd)) {
            set_result(result, "NATIVE_WIRE_SERVER_STATUS_RESPONSE_FAILED", "Native wire server could not send runtime status response.");
            return -1;
        }
        return 1;
    }
    if (strncmp(command, "update-signal", 13U) == 0) {
        if (!handle_update_signal_command(server_runtime, data_client_fd != NULL ? *data_client_fd : -1, response_fd, command, result)) {
            set_result(result, "NATIVE_WIRE_SERVER_UPDATE_SIGNAL_FAILED", "Native wire server could not update the requested signal.");
            return -1;
        }
        return 1;
    }
    if (strncmp(command, "emit-report", 11U) == 0) {
        UnitLabMmsDiagnostic diagnostic;
        unitlab_mms_diagnostic_clear(&diagnostic);
        if (data_client_fd == NULL) {
            set_result(result, "NATIVE_WIRE_SERVER_INVALID_ARGUMENT", "Native wire server requires a data client handle.");
            return -1;
        }
        if (*data_client_fd < 0) {
            struct pollfd wait_fd;
            int wait_rc;
            wait_fd.fd = data_listen_fd;
            wait_fd.events = POLLIN;
            wait_fd.revents = 0;
            wait_rc = poll(&wait_fd, 1, 2000);
            if (wait_rc < 0) {
                if (errno == EINTR) {
                    set_result(result, "NATIVE_WIRE_SERVER_DATA_ACCEPT_INTERRUPTED", "Native wire server data accept wait was interrupted.");
                    return -1;
                }
                set_result(result, "NATIVE_WIRE_SERVER_DATA_ACCEPT_FAILED", "Native wire server data accept wait failed.");
                return -1;
            }
            if (wait_rc == 0 || !(wait_fd.revents & POLLIN)) {
                set_result(result, "NATIVE_WIRE_SERVER_DATA_ACCEPT_TIMEOUT", "Native wire server did not accept a data client before emitting the report frame.");
                return -1;
            }
            *data_client_fd = accept_connection(data_listen_fd);
            if (*data_client_fd < 0) {
                set_result(result, "NATIVE_WIRE_SERVER_DATA_ACCEPT_FAILED", "Native wire server data accept failed.");
                return -1;
            }
            printf("native-wire-server: data-client-connected\n");
            fflush(stdout);
        }
        if (!build_native_information_report_frame(server_runtime, frame, frame_length, encoded_length, &diagnostic)) {
            set_result(result, "NATIVE_WIRE_SERVER_REPORT_BUILD_FAILED", diagnostic.message);
            return -1;
        }
        if (!send_all(*data_client_fd, frame, *encoded_length)) {
            set_result(result, "NATIVE_WIRE_SERVER_SEND_FAILED", "Native wire server could not send report frame.");
            return -1;
        }
        printf("native-wire-server: emitted-report bytes=%zu\n", *encoded_length);
        fflush(stdout);
        return 1;
    }
    if (strncmp(command, "emit-wire-frame", 15U) == 0) {
        UnitLabMmsDiagnostic diagnostic;
        char command_copy[128U];
        char response[8192U];
        char* token = NULL;
        char* context = NULL;
        char* frame_kind = NULL;
        unitlab_mms_diagnostic_clear(&diagnostic);
        if (frame == NULL || encoded_length == NULL) {
            set_result(result, "NATIVE_WIRE_SERVER_INVALID_ARGUMENT", "Native wire server requires frame output buffers for wire frame emission.");
            return -1;
        }
        if (strlen(command) >= sizeof(command_copy)) {
            set_result(result, "NATIVE_WIRE_SERVER_COMMAND_TOO_LONG", "Native wire server wire frame command is too long.");
            return -1;
        }
        memcpy(command_copy, command, strlen(command) + 1U);
        token = strtok_r(command_copy, " ", &context);
        token = strtok_r(NULL, " ", &context);
        frame_kind = token;
        if (frame_kind == NULL) {
            set_result(result, "NATIVE_WIRE_SERVER_FRAME_KIND_REQUIRED", "Native wire server wire frame command requires a frame kind.");
            return -1;
        }
        if (strcmp(frame_kind, "cotp-connect-request") == 0) {
            if (!unitlab_mms_build_cotp_connect_request_frame(frame, frame_length, encoded_length, &diagnostic)) {
                set_result(result, "NATIVE_WIRE_SERVER_FRAME_BUILD_FAILED", diagnostic.message);
                return -1;
            }
        }
        else if (strcmp(frame_kind, "association-request") == 0) {
            if (!unitlab_mms_build_live_wire_association_request_frame(frame, frame_length, encoded_length, &diagnostic)) {
                set_result(result, "NATIVE_WIRE_SERVER_FRAME_BUILD_FAILED", diagnostic.message);
                return -1;
            }
        }
        else if (strcmp(frame_kind, "confirmed-read-request") == 0) {
            char* domain_id = strtok_r(NULL, " ", &context);
            char* item_id = strtok_r(NULL, " ", &context);
            char* invoke_id_text = strtok_r(NULL, " ", &context);
            char* end = NULL;
            unsigned long invoke_id_value;
            uint8_t scratch[1024U];
            if (domain_id == NULL || item_id == NULL || invoke_id_text == NULL) {
                set_result(result, "NATIVE_WIRE_SERVER_FRAME_ARGS_REQUIRED", "Native wire server confirmed-read-request requires domain, item, and invoke-id arguments.");
                return -1;
            }
            invoke_id_value = strtoul(invoke_id_text, &end, 10);
            if (end == invoke_id_text || end == NULL || *end != '\0' || invoke_id_value > 0xFFFFFFFFUL) {
                set_result(result, "NATIVE_WIRE_SERVER_FRAME_INVOKE_ID_INVALID", "Native wire server confirmed-read-request requires a valid invoke-id.");
                return -1;
            }
            if (!unitlab_mms_build_read_request_frame(domain_id, item_id, (uint32_t)invoke_id_value, scratch, sizeof(scratch), frame, frame_length, encoded_length, &diagnostic)) {
                set_result(result, "NATIVE_WIRE_SERVER_FRAME_BUILD_FAILED", diagnostic.message);
                return -1;
            }
        }
        else {
            set_result(result, "NATIVE_WIRE_SERVER_FRAME_KIND_INVALID", "Native wire server wire frame command received an unsupported frame kind.");
            return -1;
        }
        if (!format_hex_response(frame, *encoded_length, response, sizeof(response))) {
            set_result(result, "NATIVE_WIRE_SERVER_FRAME_RESPONSE_FAILED", "Native wire server could not format wire frame response.");
            return -1;
        }
        if (!emit_text_response(response_fd, response)) {
            set_result(result, "NATIVE_WIRE_SERVER_FRAME_RESPONSE_FAILED", "Native wire server could not send wire frame response.");
            return -1;
        }
        printf("native-wire-server: emitted-wire-frame kind=%s bytes=%zu\n", frame_kind, *encoded_length);
        fflush(stdout);
        return 1;
    }
    if (strncmp(command, "quit", 4U) == 0 || strncmp(command, "exit", 4U) == 0) {
        printf("native-wire-server: shutdown requested\n");
        fflush(stdout);
        return 2;
    }
    return 0;
}

int unitlab_run_native_wire_server(
    UnitLabMmsServerRuntime* server_runtime,
    const UnitLabIedServerConfig* config,
    UnitLabIedModelLoadResult* result,
    UnitLabIedServerStopRequested stop_requested,
    void* stop_context)
{
    int data_listen_fd = -1;
    int control_listen_fd = -1;
    int data_client_fd = -1;
    int control_client_fd = -1;
    uint8_t frame[2048U];
    uint8_t data_rx_buffer[16384U];
    uint8_t data_cotp_rx_buffer[65535U];
    uint8_t data_cotp_frame_buffer[65535U];
    size_t frame_length = 0U;
    size_t data_rx_length = 0U;
    size_t data_cotp_rx_length = 0U;
    uint64_t next_test_tick_ms = 0U;
    uint8_t test_tick_value = 1U;
    if (result != NULL) {
        memset(result, 0, sizeof(*result));
    }
    if (server_runtime == NULL || config == NULL || result == NULL) {
        set_result(result, "NATIVE_WIRE_SERVER_INVALID_ARGUMENT", "Native wire server requires runtime, config, and result.");
        return 0;
    }
    if (server_runtime->state != UNITLAB_MMS_SERVER_RUNTIME_RUNNING) {
        set_result(result, "NATIVE_WIRE_SERVER_BAD_STATE", "Native wire server requires a running server runtime.");
        return 0;
    }
    data_listen_fd = bind_listen(config->bind_address, config->port);
    if (data_listen_fd < 0) {
        set_result(result, "NATIVE_WIRE_SERVER_LISTEN_FAILED", "Native wire server could not bind/listen on the requested data endpoint.");
        goto fail;
    }
    if (config->control_port > 0 && config->control_port != config->port) {
        control_listen_fd = bind_listen(config->bind_address, config->control_port);
        if (control_listen_fd < 0) {
            set_result(result, "NATIVE_WIRE_SERVER_CONTROL_LISTEN_FAILED", "Native wire server could not bind/listen on the requested control endpoint.");
            goto fail;
        }
    }
    set_result(result, "NATIVE_WIRE_SERVER_READY", "Native wire server is ready.");
    printf("native-wire-server: ready endpoint=%s:%d control=%d\n", config->bind_address, config->port, config->control_port);
    fflush(stdout);
    while (stop_requested == NULL || !stop_requested(stop_context)) {
        struct pollfd poll_fds[4];
        nfds_t poll_count = 0U;
        int poll_rc;
        if (data_client_fd < 0) {
            poll_fds[poll_count].fd = data_listen_fd;
            poll_fds[poll_count].events = POLLIN;
            poll_fds[poll_count].revents = 0;
            poll_count++;
        }
        if (control_client_fd < 0 && control_listen_fd >= 0) {
            poll_fds[poll_count].fd = control_listen_fd;
            poll_fds[poll_count].events = POLLIN;
            poll_fds[poll_count].revents = 0;
            poll_count++;
        }
        if (data_client_fd >= 0) {
            poll_fds[poll_count].fd = data_client_fd;
            poll_fds[poll_count].events = POLLIN;
            poll_fds[poll_count].revents = 0;
            poll_count++;
        }
        if (control_client_fd >= 0) {
            poll_fds[poll_count].fd = control_client_fd;
            poll_fds[poll_count].events = POLLIN;
            poll_fds[poll_count].revents = 0;
            poll_count++;
        }
        poll_fds[poll_count].fd = STDIN_FILENO;
        poll_fds[poll_count].events = POLLIN;
        poll_fds[poll_count].revents = 0;
        poll_count++;
        poll_rc = poll(poll_fds, poll_count, 250);
        if (poll_rc < 0) {
            if (errno == EINTR) {
                continue;
            }
            set_result(result, "NATIVE_WIRE_SERVER_POLL_FAILED", "Native wire server poll failed.");
            goto fail;
        }
        if (poll_rc == 0) {
            if (!native_wire_emit_integrity_if_due(server_runtime, data_client_fd, result)) {
                goto fail;
            }
            if (!native_wire_emit_test_tick_if_due(server_runtime, data_client_fd, config->native_test_report_tick_ms, &next_test_tick_ms, &test_tick_value, result)) {
                goto fail;
            }
            continue;
        }
        for (nfds_t index = 0U; index < poll_count; index++) {
            if (poll_fds[index].revents == 0) {
                continue;
            }
            if (!(poll_fds[index].revents & POLLIN)) {
                if (control_client_fd >= 0
                    && poll_fds[index].fd == control_client_fd
                    && (poll_fds[index].revents & (POLLHUP | POLLERR | POLLNVAL))) {
                    printf("native-wire-server: control-client-disconnected\n");
                    fflush(stdout);
                    close_fd(&control_client_fd);
                } else if (data_client_fd >= 0
                    && poll_fds[index].fd == data_client_fd
                    && (poll_fds[index].revents & (POLLHUP | POLLERR | POLLNVAL))) {
                    log_native_wire_disconnect(server_runtime, "data-client-disconnected");
                    close_fd(&data_client_fd);
                    close_fd(&control_client_fd);
                    data_rx_length = 0U;
                    data_cotp_rx_length = 0U;
                    next_test_tick_ms = 0U;
                    test_tick_value = 1U;
                    reset_native_wire_runtime_state(server_runtime);
                }
                continue;
            }
            if (data_client_fd < 0 && poll_fds[index].fd == data_listen_fd) {
                int accepted = accept_connection(data_listen_fd);
                if (accepted == -2) {
                    continue;
                }
                if (accepted < 0) {
                    set_result(result, "NATIVE_WIRE_SERVER_ACCEPT_FAILED", "Native wire server data accept failed.");
                    goto fail;
                }
                data_client_fd = accepted;
                data_rx_length = 0U;
                data_cotp_rx_length = 0U;
                next_test_tick_ms = 0U;
                test_tick_value = 1U;
                reset_native_wire_runtime_state(server_runtime);
                printf("native-wire-server: data-client-connected session-state=%u\n", (unsigned)server_runtime->session.state);
                fflush(stdout);
                continue;
            }
            if (control_client_fd < 0 && control_listen_fd >= 0 && poll_fds[index].fd == control_listen_fd) {
                int accepted = accept_connection(control_listen_fd);
                if (accepted == -2) {
                    continue;
                }
                if (accepted < 0) {
                    set_result(result, "NATIVE_WIRE_SERVER_CONTROL_ACCEPT_FAILED", "Native wire server control accept failed.");
                    goto fail;
                }
                control_client_fd = accepted;
                printf("native-wire-server: control-client-connected\n");
                fflush(stdout);
                continue;
            }
            if (data_client_fd >= 0 && poll_fds[index].fd == data_client_fd) {
                ssize_t received;

                if (data_rx_length >= sizeof(data_rx_buffer)) {
                    set_result(result, "NATIVE_WIRE_SERVER_RX_BUFFER_FULL", "Native wire server receive buffer is full.");
                    goto fail;
                }
                received = recv(data_client_fd, data_rx_buffer + data_rx_length, sizeof(data_rx_buffer) - data_rx_length, 0);
                if (received <= 0) {
                    log_native_wire_data_recv_disconnect(server_runtime, received);
                    close_fd(&data_client_fd);
                    close_fd(&control_client_fd);
                    data_rx_length = 0U;
                    data_cotp_rx_length = 0U;
                    next_test_tick_ms = 0U;
                    test_tick_value = 1U;
                    reset_native_wire_runtime_state(server_runtime);
                }
                else {
                    size_t available_length = data_rx_length + (size_t)received;

                    data_rx_length = available_length;
                    while (data_rx_length >= 4U) {
                        size_t frame_length_bytes = (size_t)(((uint16_t)data_rx_buffer[2] << 8U) | (uint16_t)data_rx_buffer[3]);

                        if (frame_length_bytes < 4U) {
                            set_result(result, "NATIVE_WIRE_SERVER_RX_TPKT_INVALID", "Native wire server received an invalid TPKT length.");
                            goto fail;
                        }
                        if (frame_length_bytes > sizeof(data_rx_buffer)) {
                            set_result(result, "NATIVE_WIRE_SERVER_RX_TPKT_TOO_LARGE", "Native wire server received a TPKT frame that exceeds the receive buffer.");
                            goto fail;
                        }
                        if (data_rx_length < frame_length_bytes) {
                            break;
                        }
                        {
                            UnitLabMmsDiagnostic cotp_diagnostic;
                            UnitLabMmsTransportFrame incoming_transport;
                            const uint8_t* process_frame = data_rx_buffer;
                            size_t process_frame_length = frame_length_bytes;
                            int should_process_frame = 1;
                            int frame_outcome = 1;

                            unitlab_mms_diagnostic_clear(&cotp_diagnostic);
                            unitlab_mms_transport_frame_init(&incoming_transport);
                            if (unitlab_mms_transport_frame_decode(&incoming_transport, data_rx_buffer, frame_length_bytes, &(size_t){0}, &cotp_diagnostic)
                                && incoming_transport.cotp.kind == UNITLAB_MMS_COTP_TPDU_DT
                                && (data_cotp_rx_length > 0U || !incoming_transport.cotp.eot)) {
                                UnitLabMmsTransportFrame reassembled_transport;
                                size_t synthetic_length = 0U;

                                if (incoming_transport.cotp.user_data_length > sizeof(data_cotp_rx_buffer) - data_cotp_rx_length) {
                                    set_result(result, "NATIVE_WIRE_SERVER_COTP_REASSEMBLY_BUFFER_FULL", "Native wire server COTP reassembly buffer is full.");
                                    goto fail;
                                }
                                if (incoming_transport.cotp.user_data_length > 0U) {
                                    memcpy(&data_cotp_rx_buffer[data_cotp_rx_length], incoming_transport.cotp.user_data, incoming_transport.cotp.user_data_length);
                                    data_cotp_rx_length += incoming_transport.cotp.user_data_length;
                                }
                                should_process_frame = incoming_transport.cotp.eot;
                                if (should_process_frame) {
                                    unitlab_mms_transport_frame_init(&reassembled_transport);
                                    reassembled_transport.cotp.kind = UNITLAB_MMS_COTP_TPDU_DT;
                                    reassembled_transport.cotp.eot = 1;
                                    reassembled_transport.cotp.user_data = data_cotp_rx_buffer;
                                    reassembled_transport.cotp.user_data_length = data_cotp_rx_length;
                                    if (!unitlab_mms_transport_frame_encode(
                                            &reassembled_transport,
                                            data_cotp_frame_buffer,
                                            sizeof(data_cotp_frame_buffer),
                                            &synthetic_length,
                                            &cotp_diagnostic)) {
                                        set_result(result, "NATIVE_WIRE_SERVER_COTP_REASSEMBLY_FAILED", cotp_diagnostic.message);
                                        goto fail;
                                    }
                                    process_frame = data_cotp_frame_buffer;
                                    process_frame_length = synthetic_length;
                                } else {
                                    printf("native-wire-server: cotp-segment-buffered bytes=%zu total=%zu eot=false\n", incoming_transport.cotp.user_data_length, data_cotp_rx_length);
                                    fflush(stdout);
                                }
                            }

                            if (should_process_frame) {
                                frame_outcome = native_wire_process_received_tpkt_frame(server_runtime, data_client_fd, process_frame, process_frame_length, result);
                                data_cotp_rx_length = 0U;
                                if (frame_outcome < 0) {
                                    goto fail;
                                }
                            }
                            if (data_rx_length > frame_length_bytes) {
                                memmove(data_rx_buffer, data_rx_buffer + frame_length_bytes, data_rx_length - frame_length_bytes);
                            }
                            data_rx_length -= frame_length_bytes;
                            if (frame_outcome == 2) {
                                log_native_wire_disconnect(server_runtime, "mms-release-complete; closing data socket");
                                close_fd(&data_client_fd);
                                close_fd(&control_client_fd);
                                data_rx_length = 0U;
                                data_cotp_rx_length = 0U;
                                next_test_tick_ms = 0U;
                                test_tick_value = 1U;
                                reset_native_wire_runtime_state(server_runtime);
                                break;
                            }
                        }
                    }
                }
                continue;
            }
            if (control_client_fd >= 0 && poll_fds[index].fd == control_client_fd) {
                char command[1024U];
                if (!read_command_from_socket(control_client_fd, command, sizeof(command))) {
                    printf("native-wire-server: control-client-disconnected\n");
                    fflush(stdout);
                    close_fd(&control_client_fd);
                    continue;
                }
                int outcome = handle_command(server_runtime, &data_client_fd, data_listen_fd, control_client_fd, command, frame, sizeof(frame), &frame_length, result);
                if (outcome < 0) {
                    goto fail;
                }
                if (outcome == 2) {
                    goto stop;
                }
                continue;
            }
            if (poll_fds[index].fd == STDIN_FILENO) {
                char command[1024U];
                if (fgets(command, sizeof(command), stdin) == NULL) {
                    continue;
                }
                int outcome = handle_command(server_runtime, &data_client_fd, data_listen_fd, -1, command, frame, sizeof(frame), &frame_length, result);
                if (outcome < 0) {
                    goto fail;
                }
                if (outcome == 2) {
                    goto stop;
                }
            }
        }
        if (!native_wire_emit_integrity_if_due(server_runtime, data_client_fd, result)) {
            goto fail;
        }
        if (!native_wire_emit_test_tick_if_due(server_runtime, data_client_fd, config->native_test_report_tick_ms, &next_test_tick_ms, &test_tick_value, result)) {
            goto fail;
        }
    }
stop:
    if (data_client_fd >= 0) {
        log_native_wire_disconnect(server_runtime, "external-stop; closing data socket");
    }
    close_fd(&control_client_fd);
    close_fd(&data_client_fd);
    close_fd(&control_listen_fd);
    close_fd(&data_listen_fd);
    set_result(result, "NATIVE_WIRE_SERVER_STOPPED", "Native wire server stopped.");
    return 1;
fail:
    close_fd(&control_client_fd);
    close_fd(&data_client_fd);
    close_fd(&control_listen_fd);
    close_fd(&data_listen_fd);
    return 0;
}
