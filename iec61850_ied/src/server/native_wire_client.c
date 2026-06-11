#define _POSIX_C_SOURCE 200112L
#include "native_wire_client.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "wire/orchestration/unitlab_mms_live_wire_probe.h"
#include "wire/orchestration/unitlab_mms_wire_builder.h"

typedef enum {
    UNITLAB_NATIVE_WIRE_CLIENT_STATE_INIT,
    UNITLAB_NATIVE_WIRE_CLIENT_STATE_DATA_CONNECTED,
    UNITLAB_NATIVE_WIRE_CLIENT_STATE_CONTROL_CONNECTED,
    UNITLAB_NATIVE_WIRE_CLIENT_STATE_COTP_CONNECTED,
    UNITLAB_NATIVE_WIRE_CLIENT_STATE_ASSOCIATING,
    UNITLAB_NATIVE_WIRE_CLIENT_STATE_ASSOCIATED,
    UNITLAB_NATIVE_WIRE_CLIENT_STATE_READY,
    UNITLAB_NATIVE_WIRE_CLIENT_STATE_READ_REQUESTED,
    UNITLAB_NATIVE_WIRE_CLIENT_STATE_GET_NAME_LIST_REQUESTED,
    UNITLAB_NATIVE_WIRE_CLIENT_STATE_WRITE_REQUESTED,
    UNITLAB_NATIVE_WIRE_CLIENT_STATE_REPORT_REQUESTED,
    UNITLAB_NATIVE_WIRE_CLIENT_STATE_STOPPED,
    UNITLAB_NATIVE_WIRE_CLIENT_STATE_FAILED,
} UnitLabNativeWireClientState;

static const char* state_name(UnitLabNativeWireClientState state)
{
    switch (state) {
    case UNITLAB_NATIVE_WIRE_CLIENT_STATE_INIT:
        return "init";
    case UNITLAB_NATIVE_WIRE_CLIENT_STATE_DATA_CONNECTED:
        return "data-connected";
    case UNITLAB_NATIVE_WIRE_CLIENT_STATE_CONTROL_CONNECTED:
        return "control-connected";
    case UNITLAB_NATIVE_WIRE_CLIENT_STATE_COTP_CONNECTED:
        return "cotp-connected";
    case UNITLAB_NATIVE_WIRE_CLIENT_STATE_ASSOCIATING:
        return "associating";
    case UNITLAB_NATIVE_WIRE_CLIENT_STATE_ASSOCIATED:
        return "associated";
    case UNITLAB_NATIVE_WIRE_CLIENT_STATE_READY:
        return "ready";
    case UNITLAB_NATIVE_WIRE_CLIENT_STATE_READ_REQUESTED:
        return "read-requested";
    case UNITLAB_NATIVE_WIRE_CLIENT_STATE_GET_NAME_LIST_REQUESTED:
        return "get-name-list-requested";
    case UNITLAB_NATIVE_WIRE_CLIENT_STATE_WRITE_REQUESTED:
        return "write-requested";
    case UNITLAB_NATIVE_WIRE_CLIENT_STATE_REPORT_REQUESTED:
        return "report-requested";
    case UNITLAB_NATIVE_WIRE_CLIENT_STATE_STOPPED:
        return "stopped";
    case UNITLAB_NATIVE_WIRE_CLIENT_STATE_FAILED:
        return "failed";
    }
    return "unknown";
}

static int emit_text_response(const char* text);

static int emit_state_response(UnitLabNativeWireClientState state)
{
    char text[96U];
    snprintf(text, sizeof(text), "native-wire-client: state=%s", state_name(state));
    return emit_text_response(text);
}

static void set_result(UnitLabIedModelLoadResult* result, const char* code, const char* message)
{
    if (result == NULL) {
        return;
    }
    result->loaded = 0;
    snprintf(result->code, sizeof(result->code), "%s", code);
    snprintf(result->message, sizeof(result->message), "%s", message);
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

static int read_exact(int fd, uint8_t* buffer, size_t length)
{
    size_t offset = 0U;
    while (offset < length) {
        ssize_t received = recv(fd, buffer + offset, length - offset, 0);
        if (received < 0) {
            if (errno == EINTR) {
                continue;
            }
            return 0;
        }
        if (received == 0) {
            return 0;
        }
        offset += (size_t)received;
    }
    return 1;
}

static int connect_socket(const char* host, int port)
{
    struct addrinfo hints;
    struct addrinfo* info = NULL;
    char port_text[16U];
    int fd = -1;

    if (host == NULL || host[0] == '\0' || port <= 0 || port > 65535) {
        return -1;
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    snprintf(port_text, sizeof(port_text), "%d", port);
    if (getaddrinfo(host, port_text, &hints, &info) != 0) {
        return -1;
    }

    for (struct addrinfo* current = info; current != NULL; current = current->ai_next) {
        fd = socket(current->ai_family, current->ai_socktype, current->ai_protocol);
        if (fd < 0) {
            continue;
        }
        if (connect(fd, current->ai_addr, current->ai_addrlen) == 0) {
            break;
        }
        close(fd);
        fd = -1;
    }

    freeaddrinfo(info);
    return fd;
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

static int emit_text_response(const char* text)
{
    printf("%s\n", text);
    fflush(stdout);
    return 1;
}

static int read_tpkt_frame(int fd, uint8_t* frame, size_t frame_length, size_t* encoded_length)
{
    uint8_t header[4U];
    uint16_t total_length;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (!read_exact(fd, header, sizeof(header))) {
        return 0;
    }
    if (header[0] != 3U || header[1] != 0U) {
        return 0;
    }

    total_length = (uint16_t)(((uint16_t)header[2] << 8U) | (uint16_t)header[3]);
    if (total_length < 4U || total_length > frame_length) {
        return 0;
    }

    memcpy(frame, header, sizeof(header));
    if (!read_exact(fd, &frame[4], (size_t)total_length - 4U)) {
        return 0;
    }
    if (encoded_length != NULL) {
        *encoded_length = (size_t)total_length;
    }
    return 1;
}

static int read_tpkt_frame_if_available(int fd, uint8_t* frame, size_t frame_length, size_t* encoded_length, int timeout_ms)
{
    fd_set read_set;
    struct timeval timeout;
    int ready;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (fd < 0 || frame == NULL || frame_length == 0U || timeout_ms < 0) {
        return -1;
    }

    FD_ZERO(&read_set);
    FD_SET(fd, &read_set);
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;

    do {
        ready = select(fd + 1, &read_set, NULL, NULL, &timeout);
    } while (ready < 0 && errno == EINTR);

    if (ready < 0) {
        return -1;
    }
    if (ready == 0) {
        return 0;
    }
    return read_tpkt_frame(fd, frame, frame_length, encoded_length) ? 1 : -1;
}

static int send_report_control_command(int control_fd, const char* command)
{
    size_t length = strlen(command);
    return send_all(control_fd, (const uint8_t*)command, length) && send_all(control_fd, (const uint8_t*)"\n", 1U);
}

static int emit_confirmed_response(
    int data_fd,
    const uint8_t* request,
    size_t request_length,
    uint8_t* response,
    size_t response_length,
    size_t* encoded_response_length,
    uint8_t* text_buffer,
    size_t text_buffer_length,
    const char* failure_message,
    UnitLabMmsDiagnostic* diagnostic)
{
    if (!send_all(data_fd, request, request_length) || !read_tpkt_frame(data_fd, response, response_length, encoded_response_length)) {
        if (diagnostic != NULL) {
            diagnostic->code = UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR;
            snprintf(diagnostic->message, sizeof(diagnostic->message), "%s", failure_message);
        }
        return 0;
    }
    if (encoded_response_length == NULL || !format_hex_response(response, *encoded_response_length, (char*)text_buffer, text_buffer_length)) {
        if (diagnostic != NULL) {
            diagnostic->code = UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL;
            snprintf(diagnostic->message, sizeof(diagnostic->message), "%s", "Native wire client could not format the confirmed response.");
        }
        return 0;
    }
    return emit_text_response((const char*)text_buffer);
}

static int emit_read_response(
    int data_fd,
    const char* domain_id,
    const char* item_id,
    uint32_t invoke_id,
    uint8_t* scratch,
    size_t scratch_length,
    uint8_t* request,
    size_t request_length,
    uint8_t* response,
    size_t response_length,
    size_t* encoded_response_length,
    uint8_t* text_buffer,
    size_t text_buffer_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    size_t encoded_request_length = 0U;

    if (domain_id == NULL || domain_id[0] == '\0' || item_id == NULL || item_id[0] == '\0') {
        if (diagnostic != NULL) {
            diagnostic->code = UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT;
            snprintf(diagnostic->message, sizeof(diagnostic->message), "%s", "Native wire client read target requires domain and item.");
        }
        return 0;
    }
    if (!unitlab_mms_build_read_request_frame(domain_id, item_id, invoke_id, scratch, scratch_length, request, request_length, &encoded_request_length, diagnostic)) {
        return 0;
    }
    return emit_confirmed_response(
        data_fd,
        request,
        encoded_request_length,
        response,
        response_length,
        encoded_response_length,
        text_buffer,
        text_buffer_length,
        "Native wire client could not receive the confirmed-read response.",
        diagnostic);
}

static int emit_get_name_list_response(
    int data_fd,
    uint32_t object_class,
    uint32_t object_scope,
    const char* domain_id,
    const char* continue_after,
    uint32_t invoke_id,
    uint8_t* scratch,
    size_t scratch_length,
    uint8_t* request,
    size_t request_length,
    uint8_t* response,
    size_t response_length,
    size_t* encoded_response_length,
    uint8_t* text_buffer,
    size_t text_buffer_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    size_t encoded_request_length = 0U;

    if (!unitlab_mms_build_get_name_list_request_frame(
            object_class,
            object_scope,
            domain_id,
            continue_after,
            invoke_id,
            scratch,
            scratch_length,
            request,
            request_length,
            &encoded_request_length,
            diagnostic)) {
        return 0;
    }
    return emit_confirmed_response(
        data_fd,
        request,
        encoded_request_length,
        response,
        response_length,
        encoded_response_length,
        text_buffer,
        text_buffer_length,
        "Native wire client could not receive the GetNameList response.",
        diagnostic);
}

static int emit_write_bool_response(
    int data_fd,
    const char* domain_id,
    const char* item_id,
    uint8_t boolean_value,
    uint32_t invoke_id,
    uint8_t* scratch,
    size_t scratch_length,
    uint8_t* request,
    size_t request_length,
    uint8_t* response,
    size_t response_length,
    size_t* encoded_response_length,
    uint8_t* text_buffer,
    size_t text_buffer_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsBerElement data_element;
    size_t encoded_request_length = 0U;

    if (domain_id == NULL || domain_id[0] == '\0' || item_id == NULL || item_id[0] == '\0') {
        if (diagnostic != NULL) {
            diagnostic->code = UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT;
            snprintf(diagnostic->message, sizeof(diagnostic->message), "%s", "Native wire client write target requires domain and item.");
        }
        return 0;
    }

    unitlab_mms_ber_element_init(&data_element);
    data_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    data_element.tag.constructed = 0;
    data_element.tag.tag_number = 3U;
    data_element.value_bytes = &boolean_value;
    data_element.value_length = 1U;

    if (!unitlab_mms_build_write_request_frame(
            domain_id,
            item_id,
            &data_element,
            invoke_id,
            scratch,
            scratch_length,
            request,
            request_length,
            &encoded_request_length,
            diagnostic)) {
        return 0;
    }
    if (!emit_confirmed_response(
            data_fd,
            request,
            encoded_request_length,
            response,
            response_length,
            encoded_response_length,
            text_buffer,
            text_buffer_length,
            "Native wire client could not receive the confirmed-write response.",
            diagnostic)) {
        return 0;
    }

    {
        int extra_frame = read_tpkt_frame_if_available(data_fd, response, response_length, encoded_response_length, 100);
        if (extra_frame < 0) {
            if (diagnostic != NULL) {
                diagnostic->code = UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR;
                snprintf(diagnostic->message, sizeof(diagnostic->message), "%s", "Native wire client could not receive an immediate post-write frame.");
            }
            return 0;
        }
        if (extra_frame > 0) {
            if (encoded_response_length == NULL || !format_hex_response(response, *encoded_response_length, (char*)text_buffer, text_buffer_length)) {
                if (diagnostic != NULL) {
                    diagnostic->code = UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL;
                    snprintf(diagnostic->message, sizeof(diagnostic->message), "%s", "Native wire client could not format the immediate post-write frame.");
                }
                return 0;
            }
            if (!emit_text_response((const char*)text_buffer)) {
                return 0;
            }
        }
    }
    return 1;
}

static int parse_uint32_token(const char* text, uint32_t* value)
{
    char* end = NULL;
    unsigned long parsed;

    if (text == NULL || value == NULL) {
        return 0;
    }
    parsed = strtoul(text, &end, 10);
    if (text == end || end == NULL || *end != '\0' || parsed > UINT32_MAX) {
        return 0;
    }
    *value = (uint32_t)parsed;
    return 1;
}

static int parse_invoke_id_token(const char* text, uint32_t* value)
{
    return parse_uint32_token(text, value) && *value != 0U;
}

static int parse_bool_token(const char* text, uint8_t* value)
{
    if (text == NULL || value == NULL) {
        return 0;
    }
    if (strcmp(text, "true") == 0 || strcmp(text, "1") == 0) {
        *value = 0x01U;
        return 1;
    }
    if (strcmp(text, "false") == 0 || strcmp(text, "0") == 0) {
        *value = 0x00U;
        return 1;
    }
    return 0;
}

int unitlab_run_native_wire_client_with_options(
    const UnitLabIedServerConfig* config,
    const UnitLabNativeWireClientOptions* options,
    UnitLabIedModelLoadResult* result,
    UnitLabIedServerStopRequested stop_requested,
    void* stop_context)
{
    int data_fd = -1;
    int control_fd = -1;
    uint8_t frame[2048U];
    uint8_t scratch[2048U];
    size_t encoded_length = 0U;
    uint8_t association_request[2048U];
    size_t association_length = 0U;
    uint8_t read_request[2048U];
    uint8_t report_frame[2048U];
    size_t report_length = 0U;
    UnitLabMmsDiagnostic diagnostic;
    const char* initial_read_domain = "XCBR1";
    const char* initial_read_item = "ST$Pos$stVal";
    uint32_t initial_read_invoke_id = 3U;
    uint32_t next_invoke_id = 4U;
    UnitLabNativeWireClientState state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_INIT;

    if (result != NULL) {
        memset(result, 0, sizeof(*result));
    }
    if (config == NULL || result == NULL) {
        set_result(result, "NATIVE_WIRE_CLIENT_INVALID_ARGUMENT", "Native wire client requires config and result.");
        return 0;
    }
    if (config->bind_address == NULL || config->bind_address[0] == '\0') {
        set_result(result, "NATIVE_WIRE_CLIENT_HOST_REQUIRED", "Native wire client requires a target host.");
        return 0;
    }
    if (config->port <= 0 || config->port > 65535 || config->control_port <= 0 || config->control_port > 65535) {
        set_result(result, "NATIVE_WIRE_CLIENT_PORT_INVALID", "Native wire client target ports are invalid.");
        return 0;
    }
    if (options != NULL) {
        if (options->initial_read_domain != NULL && options->initial_read_domain[0] != '\0') {
            initial_read_domain = options->initial_read_domain;
        }
        if (options->initial_read_item != NULL && options->initial_read_item[0] != '\0') {
            initial_read_item = options->initial_read_item;
        }
        if (options->initial_read_invoke_id != 0U) {
            initial_read_invoke_id = options->initial_read_invoke_id;
            next_invoke_id = initial_read_invoke_id + 1U;
        }
    }

    if (!emit_state_response(state)) {
        set_result(result, "NATIVE_WIRE_CLIENT_STATE_FAILED", "Native wire client could not emit its initial state.");
        return 0;
    }

    unitlab_mms_diagnostic_clear(&diagnostic);
    data_fd = connect_socket(config->bind_address, config->port);
    if (data_fd < 0) {
        state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_FAILED;
        set_result(result, "NATIVE_WIRE_CLIENT_CONNECT_FAILED", "Native wire client could not connect to the data endpoint.");
        goto fail;
    }
    state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_DATA_CONNECTED;
    if (!emit_state_response(state)) {
        set_result(result, "NATIVE_WIRE_CLIENT_STATE_FAILED", "Native wire client could not emit its data-connected state.");
        goto fail;
    }
    control_fd = connect_socket(config->bind_address, config->control_port);
    if (control_fd < 0) {
        state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_FAILED;
        set_result(result, "NATIVE_WIRE_CLIENT_CONTROL_CONNECT_FAILED", "Native wire client could not connect to the control endpoint.");
        goto fail;
    }
    state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_CONTROL_CONNECTED;
    if (!emit_state_response(state)) {
        set_result(result, "NATIVE_WIRE_CLIENT_STATE_FAILED", "Native wire client could not emit its control-connected state.");
        goto fail;
    }

    if (!unitlab_mms_build_cotp_connect_request_frame(frame, sizeof(frame), &encoded_length, &diagnostic)) {
        set_result(result, "NATIVE_WIRE_CLIENT_FRAME_BUILD_FAILED", diagnostic.message);
        goto fail;
    }
    if (!send_all(data_fd, frame, encoded_length) || !read_tpkt_frame(data_fd, frame, sizeof(frame), &encoded_length)) {
        state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_FAILED;
        set_result(result, "NATIVE_WIRE_CLIENT_COTP_EXCHANGE_FAILED", "Native wire client could not complete the COTP handshake.");
        goto fail;
    }
    state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_COTP_CONNECTED;
    if (!emit_state_response(state)) {
        set_result(result, "NATIVE_WIRE_CLIENT_STATE_FAILED", "Native wire client could not emit its cotp-connected state.");
        goto fail;
    }

    if (!unitlab_mms_build_live_wire_association_request_frame(association_request, sizeof(association_request), &association_length, &diagnostic)) {
        set_result(result, "NATIVE_WIRE_CLIENT_FRAME_BUILD_FAILED", diagnostic.message);
        goto fail;
    }
    state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_ASSOCIATING;
    if (!emit_state_response(state)) {
        set_result(result, "NATIVE_WIRE_CLIENT_STATE_FAILED", "Native wire client could not emit its associating state.");
        goto fail;
    }
    if (!send_all(data_fd, association_request, association_length) || !read_tpkt_frame(data_fd, association_request, sizeof(association_request), &association_length)) {
        state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_FAILED;
        set_result(result, "NATIVE_WIRE_CLIENT_ASSOCIATION_FAILED", "Native wire client could not complete the association handshake.");
        goto fail;
    }
    state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_ASSOCIATED;
    if (!emit_state_response(state)) {
        set_result(result, "NATIVE_WIRE_CLIENT_STATE_FAILED", "Native wire client could not emit its associated state.");
        goto fail;
    }

    state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_READ_REQUESTED;
    if (!emit_state_response(state)) {
        set_result(result, "NATIVE_WIRE_CLIENT_STATE_FAILED", "Native wire client could not emit its read-requested state.");
        goto fail;
    }
    if (!emit_read_response(
            data_fd,
            initial_read_domain,
            initial_read_item,
            initial_read_invoke_id,
            scratch,
            sizeof(scratch),
            read_request,
            sizeof(read_request),
            report_frame,
            sizeof(report_frame),
            &report_length,
            frame,
            sizeof(frame),
            &diagnostic)) {
        state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_FAILED;
        set_result(result, "NATIVE_WIRE_CLIENT_READ_FAILED", diagnostic.message);
        goto fail;
    }

    state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_READY;
    if (!emit_state_response(state)) {
        set_result(result, "NATIVE_WIRE_CLIENT_STATE_FAILED", "Native wire client could not emit its ready state.");
        goto fail;
    }
    if (!emit_text_response("native-wire-client: ready")) {
        set_result(result, "NATIVE_WIRE_CLIENT_READY_FAILED", "Native wire client could not emit its ready banner.");
        goto fail;
    }

    while (stop_requested == NULL || !stop_requested(stop_context)) {
        char command[512U];
        if (fgets(command, sizeof(command), stdin) == NULL) {
            break;
        }
        command[strcspn(command, "\r\n")] = '\0';
        if (strncmp(command, "read ", 5U) == 0) {
            char* saveptr = NULL;
            char* domain_id = strtok_r(command + 5U, " \t", &saveptr);
            char* item_id = strtok_r(NULL, " \t", &saveptr);
            char* invoke_id_text = strtok_r(NULL, " \t", &saveptr);
            char* extra = strtok_r(NULL, " \t", &saveptr);
            uint32_t invoke_id = next_invoke_id++;

            if (domain_id == NULL || item_id == NULL || extra != NULL) {
                set_result(result, "NATIVE_WIRE_CLIENT_READ_COMMAND_INVALID", "Usage: read <domain> <item> [invokeId].");
                goto fail;
            }
            if (invoke_id_text != NULL) {
                if (!parse_invoke_id_token(invoke_id_text, &invoke_id)) {
                    set_result(result, "NATIVE_WIRE_CLIENT_READ_INVOKE_INVALID", "Native wire client read invokeId must be in range 1..4294967295.");
                    goto fail;
                }
                if (invoke_id >= next_invoke_id) {
                    next_invoke_id = invoke_id + 1U;
                }
            }
            state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_READ_REQUESTED;
            if (!emit_state_response(state)) {
                set_result(result, "NATIVE_WIRE_CLIENT_STATE_FAILED", "Native wire client could not emit its read-requested state.");
                goto fail;
            }
            if (!emit_read_response(
                    data_fd,
                    domain_id,
                    item_id,
                    invoke_id,
                    scratch,
                    sizeof(scratch),
                    read_request,
                    sizeof(read_request),
                    report_frame,
                    sizeof(report_frame),
                    &report_length,
                    frame,
                    sizeof(frame),
                    &diagnostic)) {
                state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_FAILED;
                set_result(result, "NATIVE_WIRE_CLIENT_READ_FAILED", diagnostic.message);
                goto fail;
            }
            state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_READY;
            if (!emit_state_response(state)) {
                set_result(result, "NATIVE_WIRE_CLIENT_STATE_FAILED", "Native wire client could not emit its ready state after read.");
                goto fail;
            }
            continue;
        }
        if (strncmp(command, "get-name-list ", 14U) == 0) {
            char* saveptr = NULL;
            char* class_text = strtok_r(command + 14U, " \t", &saveptr);
            char* scope_text = strtok_r(NULL, " \t", &saveptr);
            char* domain_text = strtok_r(NULL, " \t", &saveptr);
            char* continue_text = strtok_r(NULL, " \t", &saveptr);
            char* invoke_id_text = strtok_r(NULL, " \t", &saveptr);
            char* extra = strtok_r(NULL, " \t", &saveptr);
            const char* domain_id = NULL;
            const char* continue_after = NULL;
            uint32_t object_class = 0U;
            uint32_t object_scope = 0U;
            uint32_t invoke_id = next_invoke_id++;

            if (class_text == NULL || scope_text == NULL || domain_text == NULL || continue_text == NULL || extra != NULL) {
                set_result(result, "NATIVE_WIRE_CLIENT_GET_NAME_LIST_COMMAND_INVALID", "Usage: get-name-list <class> <scope> <domain|-> <continueAfter|-> [invokeId].");
                goto fail;
            }
            if (!parse_uint32_token(class_text, &object_class) || !parse_uint32_token(scope_text, &object_scope)) {
                set_result(result, "NATIVE_WIRE_CLIENT_GET_NAME_LIST_ARGUMENT_INVALID", "Native wire client GetNameList class and scope must be unsigned integers.");
                goto fail;
            }
            if (strcmp(domain_text, "-") != 0) {
                domain_id = domain_text;
            }
            if (strcmp(continue_text, "-") != 0) {
                continue_after = continue_text;
            }
            if (invoke_id_text != NULL) {
                if (!parse_invoke_id_token(invoke_id_text, &invoke_id)) {
                    set_result(result, "NATIVE_WIRE_CLIENT_GET_NAME_LIST_INVOKE_INVALID", "Native wire client GetNameList invokeId must be in range 1..4294967295.");
                    goto fail;
                }
                if (invoke_id >= next_invoke_id) {
                    next_invoke_id = invoke_id + 1U;
                }
            }
            state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_GET_NAME_LIST_REQUESTED;
            if (!emit_state_response(state)) {
                set_result(result, "NATIVE_WIRE_CLIENT_STATE_FAILED", "Native wire client could not emit its get-name-list-requested state.");
                goto fail;
            }
            if (!emit_get_name_list_response(
                    data_fd,
                    object_class,
                    object_scope,
                    domain_id,
                    continue_after,
                    invoke_id,
                    scratch,
                    sizeof(scratch),
                    read_request,
                    sizeof(read_request),
                    report_frame,
                    sizeof(report_frame),
                    &report_length,
                    frame,
                    sizeof(frame),
                    &diagnostic)) {
                state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_FAILED;
                set_result(result, "NATIVE_WIRE_CLIENT_GET_NAME_LIST_FAILED", diagnostic.message);
                goto fail;
            }
            state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_READY;
            if (!emit_state_response(state)) {
                set_result(result, "NATIVE_WIRE_CLIENT_STATE_FAILED", "Native wire client could not emit its ready state after GetNameList.");
                goto fail;
            }
            continue;
        }
        if (strncmp(command, "write-bool ", 11U) == 0) {
            char* saveptr = NULL;
            char* domain_id = strtok_r(command + 11U, " \t", &saveptr);
            char* item_id = strtok_r(NULL, " \t", &saveptr);
            char* value_text = strtok_r(NULL, " \t", &saveptr);
            char* invoke_id_text = strtok_r(NULL, " \t", &saveptr);
            char* extra = strtok_r(NULL, " \t", &saveptr);
            uint8_t boolean_value = 0U;
            uint32_t invoke_id = next_invoke_id++;

            if (domain_id == NULL || item_id == NULL || value_text == NULL || extra != NULL) {
                set_result(result, "NATIVE_WIRE_CLIENT_WRITE_BOOL_COMMAND_INVALID", "Usage: write-bool <domain> <item> <true|false|1|0> [invokeId].");
                goto fail;
            }
            if (!parse_bool_token(value_text, &boolean_value)) {
                set_result(result, "NATIVE_WIRE_CLIENT_WRITE_BOOL_VALUE_INVALID", "Native wire client boolean value must be true, false, 1, or 0.");
                goto fail;
            }
            if (invoke_id_text != NULL) {
                if (!parse_invoke_id_token(invoke_id_text, &invoke_id)) {
                    set_result(result, "NATIVE_WIRE_CLIENT_WRITE_BOOL_INVOKE_INVALID", "Native wire client write invokeId must be in range 1..4294967295.");
                    goto fail;
                }
                if (invoke_id >= next_invoke_id) {
                    next_invoke_id = invoke_id + 1U;
                }
            }
            state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_WRITE_REQUESTED;
            if (!emit_state_response(state)) {
                set_result(result, "NATIVE_WIRE_CLIENT_STATE_FAILED", "Native wire client could not emit its write-requested state.");
                goto fail;
            }
            if (!emit_write_bool_response(
                    data_fd,
                    domain_id,
                    item_id,
                    boolean_value,
                    invoke_id,
                    scratch,
                    sizeof(scratch),
                    read_request,
                    sizeof(read_request),
                    report_frame,
                    sizeof(report_frame),
                    &report_length,
                    frame,
                    sizeof(frame),
                    &diagnostic)) {
                state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_FAILED;
                set_result(result, "NATIVE_WIRE_CLIENT_WRITE_BOOL_FAILED", diagnostic.message);
                goto fail;
            }
            state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_READY;
            if (!emit_state_response(state)) {
                set_result(result, "NATIVE_WIRE_CLIENT_STATE_FAILED", "Native wire client could not emit its ready state after write.");
                goto fail;
            }
            continue;
        }
        if (strcmp(command, "emit-report") == 0) {
            state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_REPORT_REQUESTED;
            if (!emit_state_response(state)) {
                set_result(result, "NATIVE_WIRE_CLIENT_STATE_FAILED", "Native wire client could not emit its report-requested state.");
                goto fail;
            }
            if (!send_report_control_command(control_fd, "emit-report")) {
                state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_FAILED;
                set_result(result, "NATIVE_WIRE_CLIENT_REPORT_COMMAND_FAILED", "Native wire client could not send the report command.");
                goto fail;
            }
            if (!read_tpkt_frame(data_fd, report_frame, sizeof(report_frame), &report_length)) {
                set_result(result, "NATIVE_WIRE_CLIENT_REPORT_FRAME_FAILED", "Native wire client could not receive the report frame.");
                goto fail;
            }
            if (!format_hex_response(report_frame, report_length, (char*)frame, sizeof(frame)) || !emit_text_response((const char*)frame)) {
                state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_FAILED;
                set_result(result, "NATIVE_WIRE_CLIENT_RESPONSE_FAILED", "Native wire client could not emit the report frame.");
                goto fail;
            }
            state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_READY;
            if (!emit_state_response(state)) {
                set_result(result, "NATIVE_WIRE_CLIENT_STATE_FAILED", "Native wire client could not emit its ready state after report.");
                goto fail;
            }
            continue;
        }
        if (strcmp(command, "exit") == 0 || strcmp(command, "quit") == 0 || strcmp(command, "stop") == 0) {
            break;
        }
    }

    close(data_fd);
    close(control_fd);
    state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_STOPPED;
    emit_state_response(state);
    set_result(result, "NATIVE_WIRE_CLIENT_STOPPED", "Native wire client stopped.");
    return 1;

fail:
    if (data_fd >= 0) {
        close(data_fd);
    }
    if (control_fd >= 0) {
        close(control_fd);
    }
    if (state != UNITLAB_NATIVE_WIRE_CLIENT_STATE_FAILED) {
        state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_FAILED;
    }
    emit_state_response(state);
    return 0;
}

int unitlab_run_native_wire_client(
    const UnitLabIedServerConfig* config,
    UnitLabIedModelLoadResult* result,
    UnitLabIedServerStopRequested stop_requested,
    void* stop_context)
{
    return unitlab_run_native_wire_client_with_options(config, NULL, result, stop_requested, stop_context);
}
