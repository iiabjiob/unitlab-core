#define _POSIX_C_SOURCE 200112L
#include "native_wire_server.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "wire/ber/unitlab_mms_ber.h"
#include "wire/mms/unitlab_mms_pdu.h"
#include "wire/transport/unitlab_mms_wire_association_fixture.h"
#include "wire/orchestration/unitlab_mms_wire_builder.h"

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

static int build_native_association_request_frame(
    UnitLabMmsServerRuntime* server_runtime,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsWireAssociationFixture fixture;
    uint8_t aarq_payload[67U] = {
        0x60U, 0x41U,
        0x80U, 0x01U, 0x00U,
        0xA1U, 0x07U, 0x06U, 0x05U, 0x28U, 0xCAU, 0x12U, 0x02U, 0x03U,
        0xBEU, 0x33U,
        0x28U, 0x31U,
        0x06U, 0x02U, 0x52U, 0x01U,
        0x02U, 0x01U, 0x03U,
        0xA0U, 0x28U,
        0xA8U, 0x26U,
        0x80U, 0x03U, 0x00U, 0xFAU, 0x00U,
        0x81U, 0x01U, 0x0AU,
        0x82U, 0x01U, 0x0AU,
        0x83U, 0x01U, 0x05U,
        0xA4U, 0x16U,
        0x80U, 0x01U, 0x01U,
        0x81U, 0x03U, 0x05U, 0xE1U, 0x00U,
        0x82U, 0x0CU, 0x03U, 0xA0U, 0x00U, 0x00U, 0x00U, 0x00U, 0x02U, 0x00U, 0x00U, 0x00U, 0xEDU, 0x10U,
    };

    unitlab_mms_wire_association_fixture_init(&fixture);
    fixture.session.kind = UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER;
    fixture.presentation.kind = UNITLAB_MMS_PRESENTATION_APDU_SIMPLY_ENCODED;
    fixture.presentation.payload_bytes = aarq_payload;
    fixture.presentation.payload_length = sizeof(aarq_payload);
    fixture.transport.cotp.kind = UNITLAB_MMS_COTP_TPDU_DT;
    fixture.transport.cotp.eot = 1;
    if (!unitlab_mms_wire_association_fixture_encode(&fixture, buffer, buffer_length, encoded_length, diagnostic)) {
        return 0;
    }
    (void)server_runtime;
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
        if (!build_native_association_request_frame(server_runtime, frame, frame_length, encoded_length, &diagnostic)) {
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
    size_t frame_length = 0U;

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
                printf("native-wire-server: data-client-connected\n");
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
                            printf("native-wire-server: data-client-connected\n");
                            fflush(stdout);
                        }
                    }
                }
                continue;
            }
            if (data_client_fd >= 0 && poll_fds[index].fd == data_client_fd) {
                uint8_t incoming[4096U];
                ssize_t received = recv(data_client_fd, incoming, sizeof(incoming), 0);
                if (received <= 0) {
                    printf("native-wire-server: data-client-disconnected\n");
                    fflush(stdout);
                    close_fd(&data_client_fd);
                    close_fd(&control_client_fd);
                }
                else {
                    UnitLabMmsOperationResult incoming_result;
                    UnitLabMmsDiagnostic response_diagnostic;
                    uint8_t response_frame[2048U];
                    size_t consumed_length = 0U;
                    size_t response_length = 0U;

                    unitlab_mms_operation_result_init(&incoming_result);
                    unitlab_mms_diagnostic_clear(&response_diagnostic);
                    if (server_runtime->session.state == UNITLAB_MMS_SESSION_DISCONNECTED
                        && unitlab_mms_server_runtime_apply_association_request_bytes(server_runtime, incoming, (size_t)received, &consumed_length, &incoming_result)) {
                        if (!unitlab_mms_build_association_response_frame(response_frame, sizeof(response_frame), &response_length, &response_diagnostic)) {
                            set_result(result, "NATIVE_WIRE_SERVER_ASSOCIATION_RESPONSE_BUILD_FAILED", response_diagnostic.message);
                            goto fail;
                        }
                        if (!send_all(data_client_fd, response_frame, response_length)) {
                            set_result(result, "NATIVE_WIRE_SERVER_ASSOCIATION_RESPONSE_SEND_FAILED", "Native wire server could not send association response frame.");
                            goto fail;
                        }
                        printf("native-wire-server: association-response-sent bytes=%zu\n", response_length);
                        fflush(stdout);
                    }
                    else {
                        printf("native-wire-server: received-bytes=%zd\n", received);
                        fflush(stdout);
                    }
                }
                continue;
            }
            if (control_client_fd >= 0 && poll_fds[index].fd == control_client_fd) {
                char command[128U];
                if (!read_command_from_socket(control_client_fd, command, sizeof(command))) {
                    printf("native-wire-server: control-client-disconnected\n");
                    fflush(stdout);
                    close_fd(&control_client_fd);
                    continue;
                }
                int outcome = handle_command(server_runtime, &data_client_fd, data_listen_fd, command, frame, sizeof(frame), &frame_length, result);
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
                int outcome = handle_command(server_runtime, &data_client_fd, data_listen_fd, command, frame, sizeof(frame), &frame_length, result);
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
