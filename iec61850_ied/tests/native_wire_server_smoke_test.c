#define _POSIX_C_SOURCE 200809L

#include "server/native_wire_server.h"
#include "server/unitlab_mms_server_runtime.h"
#include "wire/acse/unitlab_mms_acse.h"
#include "wire/ber/unitlab_mms_ber.h"
#include "wire/mms/unitlab_mms_pdu.h"
#include "wire/orchestration/unitlab_mms_wire_builder.h"
#include "wire/orchestration/unitlab_mms_association_frame.h"

#include <assert.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

typedef struct NativeWireServerTestContext {
    UnitLabMmsServerRuntime runtime;
    UnitLabIedServerConfig config;
    UnitLabIedModelLoadResult result;
    volatile int stop_requested;
    int run_result;
} NativeWireServerTestContext;

static int stop_requested_callback(void* context)
{
    return ((NativeWireServerTestContext*)context)->stop_requested != 0;
}

static void sleep_briefly(void)
{
    struct timespec request;
    request.tv_sec = 0;
    request.tv_nsec = 50U * 1000U * 1000U;
    while (nanosleep(&request, &request) != 0 && errno == EINTR) {
        continue;
    }
}

static void* run_server_thread(void* parameter)
{
    NativeWireServerTestContext* context = (NativeWireServerTestContext*)parameter;
    context->run_result = unitlab_run_native_wire_server(
        &context->runtime,
        &context->config,
        &context->result,
        stop_requested_callback,
        context);
    return NULL;
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

static int recv_exact(int fd, uint8_t* buffer, size_t length)
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

static int connect_with_retry(int port)
{
    struct sockaddr_in address;
    int fd;

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons((uint16_t)port);
    assert(inet_pton(AF_INET, "127.0.0.1", &address.sin_addr) == 1);

    for (int attempt = 0; attempt < 60; attempt++) {
        fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) {
            return -1;
        }
        if (connect(fd, (struct sockaddr*)&address, sizeof(address)) == 0) {
            return fd;
        }
        close(fd);
        sleep_briefly();
    }
    return -1;
}

static int build_association_request_bytes(uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsPdu pdu;
    UnitLabMmsAssociationFrame fixture;
    uint8_t pdu_bytes[32U];
    size_t pdu_length = 0U;
    size_t frame_length = 0U;

    unitlab_mms_diagnostic_clear(diagnostic);
    unitlab_mms_pdu_init(&pdu);
    pdu.kind = UNITLAB_MMS_PDU_INITIATE_REQUEST;
    pdu.pdu_bytes = NULL;
    pdu.pdu_length = 0U;
    if (!unitlab_mms_pdu_encode(&pdu, pdu_bytes, sizeof(pdu_bytes), &pdu_length, diagnostic)) {
        return 0;
    }

    unitlab_mms_association_frame_init(&fixture);
    {
        uint8_t presentation_bytes[64U];
        uint8_t session_bytes[128U];
        UnitLabMmsTransportFrame transport_frame;
        size_t presentation_length = 0U;
        size_t session_length = 0U;
        static const uint8_t session_prefix[] = {
            0x05U, 0x06U, 0x13U, 0x01U, 0x00U, 0x16U, 0x01U, 0x02U, 0x14U, 0x02U, 0x00U, 0x02U, 0x33U, 0x02U, 0x00U, 0x01U,
        };

        fixture.transport.cotp.kind = UNITLAB_MMS_COTP_TPDU_DT;
        fixture.transport.cotp.eot = 1;
        fixture.presentation.kind = UNITLAB_MMS_PRESENTATION_APDU_SIMPLY_ENCODED;
        fixture.presentation.payload_bytes = pdu_bytes;
        fixture.presentation.payload_length = pdu_length;
        if (!unitlab_mms_presentation_encode(&fixture.presentation, presentation_bytes, sizeof(presentation_bytes), &presentation_length, diagnostic)) {
            return 0;
        }
        if (sizeof(session_prefix) + 2U + presentation_length > sizeof(session_bytes)) {
            return 0;
        }
        session_bytes[0U] = 0x0DU;
        session_bytes[1U] = (uint8_t)(sizeof(session_prefix) + 2U + presentation_length);
        memcpy(&session_bytes[2U], session_prefix, sizeof(session_prefix));
        session_bytes[2U + sizeof(session_prefix)] = 0xC1U;
        session_bytes[3U + sizeof(session_prefix)] = (uint8_t)presentation_length;
        memcpy(&session_bytes[4U + sizeof(session_prefix)], presentation_bytes, presentation_length);
        session_length = 2U + sizeof(session_prefix) + 2U + presentation_length;
        unitlab_mms_transport_frame_init(&transport_frame);
        transport_frame.cotp = fixture.transport.cotp;
        transport_frame.cotp.user_data = session_bytes;
        transport_frame.cotp.user_data_length = session_length;
        if (!unitlab_mms_transport_frame_encode(&transport_frame, buffer, buffer_length, &frame_length, diagnostic)) {
            return 0;
        }
    }
    *encoded_length = frame_length;
    return 1;
}

static void test_native_wire_server_speaks_reference_handshake(void)
{
    NativeWireServerTestContext context;
    pthread_t thread;
    UnitLabMmsDiagnostic diagnostic;
    uint8_t client_frame[128U];
    uint8_t response_frame[256U];
    uint8_t association_request[128U];
    UnitLabMmsAssociationFrame decoded_fixture;
    size_t client_frame_length = 0U;
    size_t association_request_length = 0U;
    size_t association_response_length = 0U;
    size_t consumed_length = 0U;
    int client_fd;
    const uint8_t expected_cotp_cc[] = {
        0x03U, 0x00U, 0x00U, 0x16U, 0x11U, 0xD0U, 0x00U, 0x01U, 0x00U, 0x01U, 0x00U,
        0xC0U, 0x01U, 0x0DU, 0xC2U, 0x02U, 0x00U, 0x01U, 0xC1U, 0x02U, 0x00U, 0x01U,
    };

    memset(&context, 0, sizeof(context));
    unitlab_mms_server_runtime_init(&context.runtime);
    context.config.bind_address = "127.0.0.1";
    context.config.port = 12449;
    context.config.control_port = 0;
    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_server_runtime_prepare(&context.runtime, &context.config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&context.runtime, &diagnostic));

    assert(pthread_create(&thread, NULL, run_server_thread, &context) == 0);

    client_fd = connect_with_retry(context.config.port);
    assert(client_fd >= 0);

    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_build_cotp_connect_request_frame(client_frame, sizeof(client_frame), &client_frame_length, &diagnostic));
    assert(client_frame_length == sizeof(expected_cotp_cc));
    assert(send_all(client_fd, client_frame, client_frame_length));
    assert(recv_exact(client_fd, response_frame, sizeof(expected_cotp_cc)));
    assert(memcmp(response_frame, expected_cotp_cc, sizeof(expected_cotp_cc)) == 0);

    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(build_association_request_bytes(association_request, sizeof(association_request), &association_request_length, &diagnostic));
    assert(send_all(client_fd, association_request, association_request_length));
    assert(recv_exact(client_fd, response_frame, 4U));
    association_response_length = ((size_t)response_frame[2] << 8U) | (size_t)response_frame[3];
    assert(association_response_length <= sizeof(response_frame));
    assert(recv_exact(client_fd, &response_frame[4U], association_response_length - 4U));
    assert(unitlab_mms_association_frame_decode(&decoded_fixture, response_frame, association_response_length, &consumed_length, &diagnostic));
    assert(consumed_length == association_response_length);
    assert(decoded_fixture.transport.cotp.kind == UNITLAB_MMS_COTP_TPDU_DT);
    assert(decoded_fixture.session.kind == UNITLAB_MMS_SESSION_SPDU_ACCEPT);
    assert(decoded_fixture.presentation.kind == UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED);

    assert(decoded_fixture.presentation.payload_length > 0U);

    close(client_fd);
    context.stop_requested = 1;
    assert(pthread_join(thread, NULL) == 0);
    assert(context.run_result == 1);
    assert(strcmp(context.result.code, "NATIVE_WIRE_SERVER_STOPPED") == 0);
}

int main(void)
{
    test_native_wire_server_speaks_reference_handshake();
    return 0;
}
