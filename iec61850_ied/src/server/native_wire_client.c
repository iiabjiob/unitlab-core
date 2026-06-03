#define _POSIX_C_SOURCE 200112L
#include "native_wire_client.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

static int send_report_control_command(int control_fd, const char* command)
{
    size_t length = strlen(command);
    return send_all(control_fd, (const uint8_t*)command, length) && send_all(control_fd, (const uint8_t*)"\n", 1U);
}

int unitlab_run_native_wire_client(
    const UnitLabIedServerConfig* config,
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
    size_t read_length = 0U;
    uint8_t report_frame[2048U];
    size_t report_length = 0U;
    UnitLabMmsDiagnostic diagnostic;
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

    if (!unitlab_mms_build_read_request_frame("XCBR1", "ST$Pos$stVal", 3U, scratch, sizeof(scratch), read_request, sizeof(read_request), &read_length, &diagnostic)) {
        set_result(result, "NATIVE_WIRE_CLIENT_FRAME_BUILD_FAILED", diagnostic.message);
        goto fail;
    }
    if (!send_all(data_fd, read_request, read_length) || !read_tpkt_frame(data_fd, report_frame, sizeof(report_frame), &report_length)) {
        state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_FAILED;
        set_result(result, "NATIVE_WIRE_CLIENT_READ_FAILED", "Native wire client could not receive the initial confirmed-read response.");
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
    if (!format_hex_response(report_frame, report_length, (char*)frame, sizeof(frame)) || !emit_text_response((const char*)frame)) {
        set_result(result, "NATIVE_WIRE_CLIENT_RESPONSE_FAILED", "Native wire client could not emit the initial confirmed-read response.");
        goto fail;
    }

    while (stop_requested == NULL || !stop_requested(stop_context)) {
        char command[128U];
        if (fgets(command, sizeof(command), stdin) == NULL) {
            break;
        }
        command[strcspn(command, "\r\n")] = '\0';
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
