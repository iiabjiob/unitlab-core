#define _POSIX_C_SOURCE 200112L
#include "native_wire_server.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <poll.h>
#include <signal.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "model/model_loader.h"
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
    uint8_t response_frame[2048U];
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
        && server_runtime->pending_request.state == UNITLAB_MMS_PENDING_REQUEST_ACTIVE
        && server_runtime->pending_request.last_event.kind == UNITLAB_MMS_RUNTIME_EVENT_REQUEST_STARTED) {
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
        if (!send_all(data_client_fd, response_frame, response_length)) {
            set_result(result, "NATIVE_WIRE_SERVER_RESPONSE_SEND_FAILED", "Native wire server could not send confirmed response frame.");
            return -1;
        }
        if (!unitlab_mms_pending_request_complete(&server_runtime->pending_request, native_wire_now_ms(), &response_diagnostic)) {
            set_result(result, "NATIVE_WIRE_SERVER_REQUEST_COMPLETE_FAILED", response_diagnostic.message);
            return -1;
        }
        printf("native-wire-server: confirmed-response-sent bytes=%zu\n", response_length);
        fflush(stdout);
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
                if (send_all(data_client_fd, response_frame, response_length)) {
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
    if (client_fd < 0) {
        if (errno == EINTR) {
            return -2;
        }
        return -1;
    }
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
    size_t frame_length = 0U;
    size_t data_rx_length = 0U;
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
            continue;
        }
        for (nfds_t index = 0U; index < poll_count; index++) {
            if (!(poll_fds[index].revents & POLLIN)) {
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
                if (data_client_fd < 0) {
                    struct pollfd wait_fd;
                    int wait_rc;
                    wait_fd.fd = data_listen_fd;
                    wait_fd.events = POLLIN;
                    wait_fd.revents = 0;
                    wait_rc = poll(&wait_fd, 1, 2000);
                    if (wait_rc > 0 && (wait_fd.revents & POLLIN)) {
                        int retried = accept_connection(data_listen_fd);
                        if (retried > 0) {
                            data_client_fd = retried;
                            data_rx_length = 0U;
                            printf("native-wire-server: data-client-connected\n");
                            fflush(stdout);
                        }
                    }
                }
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
                        if (native_wire_process_received_tpkt_frame(server_runtime, data_client_fd, data_rx_buffer, frame_length_bytes, result) < 0) {
                            goto fail;
                        }
                        if (data_rx_length > frame_length_bytes) {
                            memmove(data_rx_buffer, data_rx_buffer + frame_length_bytes, data_rx_length - frame_length_bytes);
                        }
                        data_rx_length -= frame_length_bytes;
                    }
                }
                continue;
            }
            if (control_client_fd >= 0 && poll_fds[index].fd == control_client_fd) {
                char command[128U];
                if (!read_command_from_socket(control_client_fd, command, sizeof(command))) {
                    log_native_wire_disconnect(server_runtime, "control-client-disconnected; closing data socket due to control disconnect");
                    close_fd(&control_client_fd);
                    close_fd(&data_client_fd);
                    data_rx_length = 0U;
                    reset_native_wire_runtime_state(server_runtime);
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
                char command[128U];
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
