#define _POSIX_C_SOURCE 200809L

#include "native_wire_server.h"
#include "unitlab_mms_server_runtime.h"
#include "wire/acse/unitlab_mms_acse.h"
#include "wire/mms/unitlab_mms_pdu.h"
#include "wire/orchestration/unitlab_mms_wire_builder.h"
#include "wire/transport/unitlab_mms_wire_association_fixture.h"

#include <assert.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
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
    UnitLabMmsWireAssociationFixture fixture;
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

    unitlab_mms_wire_association_fixture_init(&fixture);
    fixture.transport.cotp.kind = UNITLAB_MMS_COTP_TPDU_DT;
    fixture.transport.cotp.eot = 1;
    fixture.session.kind = UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER;
    fixture.presentation.kind = UNITLAB_MMS_PRESENTATION_APDU_SIMPLY_ENCODED;
    fixture.presentation.payload_bytes = pdu_bytes;
    fixture.presentation.payload_length = pdu_length;
    if (!unitlab_mms_wire_association_fixture_encode(&fixture, buffer, buffer_length, &frame_length, diagnostic)) {
        return 0;
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
    size_t client_frame_length = 0U;
    size_t association_request_length = 0U;
    int client_fd;
    const uint8_t expected_cotp_cc[] = {
        0x03U, 0x00U, 0x00U, 0x16U, 0x11U, 0xD0U, 0x00U, 0x01U, 0x00U, 0x01U, 0x00U,
        0xC0U, 0x01U, 0x0DU, 0xC2U, 0x02U, 0x00U, 0x01U, 0xC1U, 0x02U, 0x00U, 0x01U,
    };
    const uint8_t expected_association_response[] = {
        0x03U, 0x00U, 0x00U, 0x8FU, 0x02U, 0xF0U, 0x80U, 0x0EU, 0x86U, 0x05U, 0x06U, 0x13U, 0x01U, 0x00U, 0x16U, 0x01U,
        0x02U, 0x14U, 0x02U, 0x00U, 0x02U, 0x34U, 0x02U, 0x00U, 0x01U, 0xC1U, 0x74U, 0x31U, 0x72U, 0xA0U, 0x03U, 0x80U, 0x01U,
        0x01U, 0xA2U, 0x6BU, 0x83U, 0x04U, 0x00U, 0x00U, 0x00U, 0x01U, 0xA5U, 0x12U, 0x30U, 0x07U, 0x80U, 0x01U, 0x00U,
        0x81U, 0x02U, 0x51U, 0x01U, 0x30U, 0x07U, 0x80U, 0x01U, 0x00U, 0x81U, 0x02U, 0x51U, 0x01U, 0x61U, 0x4FU, 0x30U,
        0x4DU, 0x02U, 0x01U, 0x01U, 0xA0U, 0x48U, 0x61U, 0x46U, 0xA1U, 0x07U, 0x06U, 0x05U, 0x28U, 0xCAU, 0x22U, 0x02U,
        0x03U, 0xA2U, 0x03U, 0x02U, 0x01U, 0x00U, 0xA3U, 0x05U, 0xA1U, 0x03U, 0x02U, 0x01U, 0x00U, 0xBEU, 0x2FU, 0x28U,
        0x2DU, 0x02U, 0x01U, 0x03U, 0xA0U, 0x28U, 0xA9U, 0x26U, 0x80U, 0x03U, 0x00U, 0xFDU, 0xE8U, 0x81U, 0x01U, 0x05U,
        0x82U, 0x01U, 0x05U, 0x83U, 0x01U, 0x0AU, 0xA4U, 0x16U, 0x80U, 0x01U, 0x01U, 0x81U, 0x03U, 0x05U, 0xF1U, 0x00U,
        0x82U, 0x0CU, 0x03U, 0xEEU, 0x1CU, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x01U, 0x18U,
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
    assert(recv_exact(client_fd, response_frame, sizeof(expected_association_response)));
    assert(memcmp(response_frame, expected_association_response, sizeof(expected_association_response)) == 0);

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
