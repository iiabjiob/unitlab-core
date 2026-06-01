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
#include "wire/orchestration/unitlab_mms_wire_builder.h"
#include "wire/mms/unitlab_mms_pdu.h"

static void set_result(UnitLabIedModelLoadResult* result, const char* code, const char* message)
{
    if (result == NULL) {
        return;
    }
    result->loaded = 0;
    snprintf(result->code, sizeof(result->code), "%s", code);
    snprintf(result->message, sizeof(result->message), "%s", message);
}

static int build_empty_information_report_frame(
    UnitLabMmsServerRuntime* server_runtime,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsBerElement report_element;
    UnitLabMmsPdu report_pdu;
    uint8_t service_bytes[16U];
    size_t service_length = 0U;

    unitlab_mms_ber_element_init(&report_element);
    unitlab_mms_ber_tag_init(&report_element.tag);
    report_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    report_element.tag.constructed = 1;
    report_element.tag.tag_number = 0U;
    report_element.value_bytes = NULL;
    report_element.value_length = 0U;
    if (!unitlab_mms_ber_write(&report_element, service_bytes, sizeof(service_bytes), &service_length, diagnostic)) {
        return 0;
    }

    unitlab_mms_pdu_init(&report_pdu);
    report_pdu.kind = UNITLAB_MMS_PDU_UNCONFIRMED;
    report_pdu.has_service = 1;
    report_pdu.service_kind = UNITLAB_MMS_SERVICE_INFORMATION_REPORT;
    report_pdu.pdu_bytes = service_bytes;
    report_pdu.pdu_length = service_length;
    return unitlab_mms_build_wire_frame_from_pdu(
        &report_pdu,
        server_runtime->wire_scratch,
        sizeof(server_runtime->wire_scratch),
        buffer,
        buffer_length,
        encoded_length,
        diagnostic);
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

static int resolve_listener(const UnitLabIedServerConfig* config, struct addrinfo** out_info)
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
    snprintf(port_text, sizeof(port_text), "%d", config->port);
    status = getaddrinfo(config->bind_address, port_text, &hints, out_info);
    return status == 0;
}

int unitlab_run_native_wire_server(
    UnitLabMmsServerRuntime* server_runtime,
    const UnitLabIedServerConfig* config,
    UnitLabIedModelLoadResult* result,
    UnitLabIedServerStopRequested stop_requested,
    void* stop_context)
{
    struct addrinfo* listener_info = NULL;
    int listen_fd = -1;
    int client_fd = -1;
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
    if (!resolve_listener(config, &listener_info)) {
        set_result(result, "NATIVE_WIRE_SERVER_RESOLVE_FAILED", "Native wire server could not resolve bind address.");
        return 0;
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
    if (listen_fd < 0) {
        set_result(result, "NATIVE_WIRE_SERVER_LISTEN_FAILED", "Native wire server could not bind/listen on the requested endpoint.");
        return 0;
    }

    set_result(result, "NATIVE_WIRE_SERVER_READY", "Native wire server is ready.");
    printf("native-wire-server: ready endpoint=%s:%d\n", config->bind_address, config->port);
    fflush(stdout);

    while (stop_requested == NULL || !stop_requested(stop_context)) {
        struct pollfd server_poll;
        int poll_rc;

        server_poll.fd = listen_fd;
        server_poll.events = POLLIN;
        server_poll.revents = 0;
        poll_rc = poll(&server_poll, 1, 250);
        if (poll_rc < 0) {
            if (errno == EINTR) {
                continue;
            }
            set_result(result, "NATIVE_WIRE_SERVER_POLL_FAILED", "Native wire server poll failed.");
            close(listen_fd);
            return 0;
        }
        if (poll_rc == 0) {
            continue;
        }
        client_fd = accept(listen_fd, NULL, NULL);
        if (client_fd < 0) {
            if (errno == EINTR) {
                continue;
            }
            set_result(result, "NATIVE_WIRE_SERVER_ACCEPT_FAILED", "Native wire server accept failed.");
            close(listen_fd);
            return 0;
        }
        printf("native-wire-server: client-connected\n");
        fflush(stdout);
        break;
    }

    if (client_fd < 0) {
        close(listen_fd);
        return 1;
    }

    while (stop_requested == NULL || !stop_requested(stop_context)) {
        struct pollfd poll_fds[2];
        char command[128U];
        int poll_rc;

        poll_fds[0].fd = client_fd;
        poll_fds[0].events = POLLIN;
        poll_fds[0].revents = 0;
        poll_fds[1].fd = STDIN_FILENO;
        poll_fds[1].events = POLLIN;
        poll_fds[1].revents = 0;
        poll_rc = poll(poll_fds, 2, 250);
        if (poll_rc < 0) {
            if (errno == EINTR) {
                continue;
            }
            set_result(result, "NATIVE_WIRE_SERVER_POLL_FAILED", "Native wire server client poll failed.");
            close(client_fd);
            close(listen_fd);
            return 0;
        }
        if (poll_fds[1].revents & POLLIN) {
            if (fgets(command, sizeof(command), stdin) != NULL) {
                if (strncmp(command, "emit-report", 11U) == 0) {
                    UnitLabMmsDiagnostic diagnostic;
                    unitlab_mms_diagnostic_clear(&diagnostic);
                    if (build_empty_information_report_frame(server_runtime, frame, sizeof(frame), &frame_length, &diagnostic)) {
                        if (send_all(client_fd, frame, frame_length)) {
                            printf("native-wire-server: emitted-report bytes=%zu\n", frame_length);
                            fflush(stdout);
                        } else {
                            set_result(result, "NATIVE_WIRE_SERVER_SEND_FAILED", "Native wire server could not send report frame.");
                            close(client_fd);
                            close(listen_fd);
                            return 0;
                        }
                    } else {
                        set_result(result, "NATIVE_WIRE_SERVER_REPORT_BUILD_FAILED", diagnostic.message);
                        close(client_fd);
                        close(listen_fd);
                        return 0;
                    }
                }
                else if (strncmp(command, "quit", 4U) == 0 || strncmp(command, "exit", 4U) == 0) {
                    printf("native-wire-server: shutdown requested\n");
                    fflush(stdout);
                    break;
                }
            }
        }
        if (poll_fds[0].revents & POLLIN) {
            uint8_t incoming[4096U];
            ssize_t received = recv(client_fd, incoming, sizeof(incoming), 0);
            if (received <= 0) {
                printf("native-wire-server: client-disconnected\n");
                fflush(stdout);
                break;
            }
            printf("native-wire-server: received-bytes=%zd\n", received);
            fflush(stdout);
        }
    }

    close(client_fd);
    close(listen_fd);
    set_result(result, "NATIVE_WIRE_SERVER_STOPPED", "Native wire server stopped.");
    return 1;
}
