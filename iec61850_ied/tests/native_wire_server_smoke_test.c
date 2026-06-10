#define _POSIX_C_SOURCE 200809L

#include "server/native_wire_server.h"
#include "server/unitlab_mms_server_runtime.h"
#include "model/model_plan.h"
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

static int recv_line(int fd, char* buffer, size_t buffer_size)
{
    size_t offset = 0U;

    if (buffer == NULL || buffer_size == 0U) {
        return 0;
    }
    while (offset + 1U < buffer_size) {
        char byte = 0;
        ssize_t received = recv(fd, &byte, 1U, 0);
        if (received < 0) {
            if (errno == EINTR) {
                continue;
            }
            return 0;
        }
        if (received == 0) {
            break;
        }
        if (byte == '\n') {
            break;
        }
        buffer[offset++] = byte;
    }
    buffer[offset] = '\0';
    return offset > 0U;
}

static int send_line(int fd, const char* text)
{
    return text != NULL
        && send_all(fd, (const uint8_t*)text, strlen(text))
        && send_all(fd, (const uint8_t*)"\n", 1U);
}

static void hex_encode_text(const char* text, char* output, size_t output_size)
{
    static const char hex_digits[] = "0123456789abcdef";
    size_t length;

    assert(text != NULL);
    assert(output != NULL);
    length = strlen(text);
    assert(output_size >= length * 2U + 1U);
    for (size_t index = 0U; index < length; index++) {
        unsigned char value = (unsigned char)text[index];
        output[index * 2U] = hex_digits[(value >> 4U) & 0x0FU];
        output[index * 2U + 1U] = hex_digits[value & 0x0FU];
    }
    output[length * 2U] = '\0';
}

static int contains_bytes(const uint8_t* haystack, size_t haystack_length, const uint8_t* needle, size_t needle_length)
{
    if (haystack == NULL || needle == NULL || needle_length == 0U || haystack_length < needle_length) {
        return 0;
    }
    for (size_t index = 0U; index + needle_length <= haystack_length; index++) {
        if (memcmp(&haystack[index], needle, needle_length) == 0) {
            return 1;
        }
    }
    return 0;
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
    uint8_t association_request[256U];
    UnitLabMmsAssociationFrame decoded_fixture;
    size_t client_frame_length = 0U;
    size_t association_request_length = 0U;
    size_t association_response_length = 0U;
    size_t consumed_length = 0U;
    int client_fd;
    /* Golden association-accept frame captured from libIEC61850 server_example_basic_io. */
    const uint8_t expected_cotp_cc[] = {
        0x03U, 0x00U, 0x00U, 0x16U, 0x11U, 0xD0U, 0x00U, 0x01U, 0x00U, 0x01U, 0x00U,
        0xC0U, 0x01U, 0x0AU, 0xC2U, 0x02U, 0x00U, 0x01U, 0xC1U, 0x02U, 0x00U, 0x01U,
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

    {
        uint8_t read_request[256U];
        uint8_t read_request_2[256U];
        uint8_t scratch[1024U];
        uint8_t coalesced_requests[512U];
        uint8_t response_a[1024U];
        uint8_t response_b[1024U];
        UnitLabMmsAssociationFrame response_fixture;
        UnitLabMmsPdu decoded_response_pdu;
        size_t read_request_length = 0U;
        size_t read_request_2_length = 0U;
        size_t coalesced_request_length = 0U;
        size_t response_a_length = 0U;
        size_t response_b_length = 0U;
        size_t response_pdu_consumed_length = 0U;
        size_t response_frame_consumed_length = 0U;

        unitlab_mms_diagnostic_clear(&diagnostic);
        assert(unitlab_mms_build_read_request_frame("LD0", "LLN0$EX$NamPlt$ldNs", 10U, scratch, sizeof(scratch), read_request, sizeof(read_request), &read_request_length, &diagnostic));
        assert(unitlab_mms_build_read_request_frame("LD0", "LLN0$DC$NamPlt$vendor", 11U, scratch, sizeof(scratch), read_request_2, sizeof(read_request_2), &read_request_2_length, &diagnostic));
        assert(read_request_length + read_request_2_length <= sizeof(coalesced_requests));
        memcpy(coalesced_requests, read_request, read_request_length);
        memcpy(coalesced_requests + read_request_length, read_request_2, read_request_2_length);
        coalesced_request_length = read_request_length + read_request_2_length;
        assert(send_all(client_fd, coalesced_requests, coalesced_request_length));

        assert(recv_exact(client_fd, response_a, 4U));
        response_a_length = ((size_t)response_a[2] << 8U) | (size_t)response_a[3];
        assert(response_a_length <= sizeof(response_a));
        assert(recv_exact(client_fd, &response_a[4U], response_a_length - 4U));
        assert(unitlab_mms_association_frame_decode(&response_fixture, response_a, response_a_length, &response_frame_consumed_length, &diagnostic));
        assert(response_frame_consumed_length == response_a_length);
        assert(unitlab_mms_pdu_decode(&decoded_response_pdu, response_fixture.presentation.payload_bytes, response_fixture.presentation.payload_length, &response_pdu_consumed_length, &diagnostic));
        assert(response_pdu_consumed_length == response_fixture.presentation.payload_length);
        assert(decoded_response_pdu.kind == UNITLAB_MMS_PDU_CONFIRMED_RESPONSE);
        assert(decoded_response_pdu.invoke_id == 10U);
        assert(decoded_response_pdu.service_kind == UNITLAB_MMS_SERVICE_READ);

        assert(recv_exact(client_fd, response_b, 4U));
        response_b_length = ((size_t)response_b[2] << 8U) | (size_t)response_b[3];
        assert(response_b_length <= sizeof(response_b));
        assert(recv_exact(client_fd, &response_b[4U], response_b_length - 4U));
        assert(unitlab_mms_association_frame_decode(&response_fixture, response_b, response_b_length, &response_frame_consumed_length, &diagnostic));
        assert(response_frame_consumed_length == response_b_length);
        assert(unitlab_mms_pdu_decode(&decoded_response_pdu, response_fixture.presentation.payload_bytes, response_fixture.presentation.payload_length, &response_pdu_consumed_length, &diagnostic));
        assert(response_pdu_consumed_length == response_fixture.presentation.payload_length);
        assert(decoded_response_pdu.kind == UNITLAB_MMS_PDU_CONFIRMED_RESPONSE);
        assert(decoded_response_pdu.invoke_id == 11U);
        assert(decoded_response_pdu.service_kind == UNITLAB_MMS_SERVICE_READ);
        assert(response_fixture.presentation.payload_length > 0U);
        assert(contains_bytes(response_fixture.presentation.payload_bytes, response_fixture.presentation.payload_length, (const uint8_t*)"UnitLab", strlen("UnitLab")) == 1);
    }

    close(client_fd);
    context.stop_requested = 1;
    assert(pthread_join(thread, NULL) == 0);
    assert(context.run_result == 1);
    assert(strcmp(context.result.code, "NATIVE_WIRE_SERVER_STOPPED") == 0);
}


static void test_native_wire_server_control_status_and_signal_update(void)
{
    NativeWireServerTestContext context;
    pthread_t thread;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabIedModelPlan plan;
    UnitLabIedModelDataSet data_sets[1];
    UnitLabIedModelReportControl reports[1];
    UnitLabIedModelSignal signals[1];
    char response[2048U];
    char reference_hex[512U];
    char value_hex[32U];
    char command[768U];
    int control_fd;

    memset(&context, 0, sizeof(context));
    memset(&plan, 0, sizeof(plan));
    memset(data_sets, 0, sizeof(data_sets));
    memset(reports, 0, sizeof(reports));
    memset(signals, 0, sizeof(signals));

    snprintf(data_sets[0].reference, sizeof(data_sets[0].reference), "%s", "IED1LD0/LLN0$dsEvents");
    snprintf(data_sets[0].logical_device_inst, sizeof(data_sets[0].logical_device_inst), "%s", "IED1LD0");
    snprintf(data_sets[0].logical_node_name, sizeof(data_sets[0].logical_node_name), "%s", "LLN0");
    snprintf(data_sets[0].name, sizeof(data_sets[0].name), "%s", "dsEvents");
    data_sets[0].first_signal_index = 0U;
    data_sets[0].member_count = 1U;

    snprintf(reports[0].key, sizeof(reports[0].key), "%s", "IED1LD0/LLN0/brcbEvents/buffered");
    snprintf(reports[0].logical_device_inst, sizeof(reports[0].logical_device_inst), "%s", "IED1LD0");
    snprintf(reports[0].logical_node_name, sizeof(reports[0].logical_node_name), "%s", "LLN0");
    snprintf(reports[0].name, sizeof(reports[0].name), "%s", "brcbEvents");
    snprintf(reports[0].report_kind, sizeof(reports[0].report_kind), "%s", "buffered");
    snprintf(reports[0].rpt_id, sizeof(reports[0].rpt_id), "%s", "IED1LD0/LLN0.BR.Events");
    snprintf(reports[0].data_set_ref, sizeof(reports[0].data_set_ref), "%s", "IED1LD0/LLN0$dsEvents");
    reports[0].is_buffered = 1;
    reports[0].data_set_index = 0U;

    snprintf(signals[0].reference, sizeof(signals[0].reference), "%s", "LD0/PGGIO1.Ind1.stVal[ST]");
    snprintf(signals[0].kind, sizeof(signals[0].kind), "%s", "FCDA");
    signals[0].data_set_index = 0U;
    signals[0].member_index = 0U;
    snprintf(signals[0].logical_device_inst, sizeof(signals[0].logical_device_inst), "%s", "IED1LD0");
    snprintf(signals[0].logical_node_name, sizeof(signals[0].logical_node_name), "%s", "PGGIO1");
    snprintf(signals[0].data_object_name, sizeof(signals[0].data_object_name), "%s", "Ind1");
    snprintf(signals[0].data_attribute_path, sizeof(signals[0].data_attribute_path), "%s", "stVal");
    snprintf(signals[0].object_reference, sizeof(signals[0].object_reference), "%s", "IED1LD0.PGGIO1.Ind1.stVal");
    snprintf(signals[0].data_set_entry_variable, sizeof(signals[0].data_set_entry_variable), "%s", "IED1LD0/PGGIO1$ST$Ind1$stVal");
    snprintf(signals[0].fc, sizeof(signals[0].fc), "%s", "ST");
    signals[0].initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_INTEGER;
    snprintf(signals[0].initial_value, sizeof(signals[0].initial_value), "%s", "1");

    plan.data_set_count = 1U;
    plan.data_sets = data_sets;
    plan.report_count = 1U;
    plan.reports = reports;
    plan.signal_count = 1U;
    plan.signals = signals;

    unitlab_mms_server_runtime_init(&context.runtime);
    context.config.bind_address = "127.0.0.1";
    context.config.port = 12459;
    context.config.control_port = 12460;
    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_server_runtime_prepare(&context.runtime, &context.config, &diagnostic));
    assert(unitlab_mms_server_runtime_apply_model_plan(&context.runtime, &plan));
    assert(unitlab_mms_server_runtime_start(&context.runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_reserve_report_control(&context.runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_enable_report_control(&context.runtime, &diagnostic));
    context.runtime.brcb_rpt_ena = 1U;

    assert(pthread_create(&thread, NULL, run_server_thread, &context) == 0);

    control_fd = connect_with_retry(context.config.control_port);
    assert(control_fd >= 0);
    assert(send_line(control_fd, "status"));
    assert(recv_line(control_fd, response, sizeof(response)));
    assert(strstr(response, "\"ok\":true") != NULL);
    assert(strstr(response, "\"dataClientConnected\":false") != NULL);
    assert(strstr(response, "\"reportEnabled\":true") != NULL);
    close(control_fd);
    sleep_briefly();

    hex_encode_text("IED1LD0/PGGIO1$ST$Ind1$stVal", reference_hex, sizeof(reference_hex));
    hex_encode_text("11", value_hex, sizeof(value_hex));
    snprintf(command, sizeof(command), "update-signal integer %s %s", reference_hex, value_hex);
    control_fd = connect_with_retry(context.config.control_port);
    assert(control_fd >= 0);
    assert(send_line(control_fd, command));
    assert(recv_line(control_fd, response, sizeof(response)));
    assert(strstr(response, "\"ok\":true") != NULL);
    assert(strstr(response, "\"reportQueued\":true") != NULL);
    assert(strstr(response, "\"reportSent\":false") != NULL);
    assert(context.runtime.pending_report_kind == UNITLAB_MMS_SERVER_PENDING_REPORT_DATA_CHANGE);
    close(control_fd);

    context.stop_requested = 1;
    assert(pthread_join(thread, NULL) == 0);
    assert(context.run_result == 1);
    assert(strcmp(context.result.code, "NATIVE_WIRE_SERVER_STOPPED") == 0);
}

int main(void)
{
    test_native_wire_server_speaks_reference_handshake();
    test_native_wire_server_control_status_and_signal_update();
    return 0;
}
