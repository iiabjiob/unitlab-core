#include "client_probe.h"
#include "model_loader.h"

#include <hal_thread.h>
#include <iec61850_client.h>

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

typedef struct ServerThreadContext {
    const UnitLabIedFixtureModel* fixture;
    const UnitLabIedModelPlan* plan;
    UnitLabIedServerConfig config;
    volatile int stop_requested;
    int run_result;
    UnitLabIedModelLoadResult load_result;
} ServerThreadContext;

static int expect_true(int condition, const char* message)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
    }
    return condition;
}

static int expect_string(const char* actual, const char* expected, const char* message)
{
    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "FAIL: %s: expected \"%s\", got \"%s\"\n", message, expected, actual);
        return 0;
    }
    return 1;
}

static int expect_string_contains(const char* actual, const char* expected, const char* message)
{
    if (actual == NULL || strstr(actual, expected) == NULL) {
        fprintf(stderr, "FAIL: %s: expected \"%s\" to contain \"%s\"\n", message, actual == NULL ? "<null>" : actual, expected);
        return 0;
    }
    return 1;
}

static int string_list_contains(LinkedList list, const char* expected)
{
    for (LinkedList entry = LinkedList_getNext(list); entry != NULL; entry = LinkedList_getNext(entry)) {
        const char* value = (const char*)LinkedList_getData(entry);
        if (value != NULL && strcmp(value, expected) == 0) {
            return 1;
        }
    }
    return 0;
}

static UnitLabIedFixtureModel valid_fixture(
    UnitLabIedFixtureDataSet* data_sets,
    UnitLabIedFixtureReport* reports,
    UnitLabIedFixtureSignal* signals)
{
    signals[0] = (UnitLabIedFixtureSignal){
        .data_set_index = 0U,
        .reference = "LD0/XCBR1.Pos.stVal[ST]",
        .kind = "FCDA",
        .fc = "ST",
        .initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_INTEGER,
        .initial_value = "1",
    };
    data_sets[0] = (UnitLabIedFixtureDataSet){
        .reference = "IED1/AP1/LD0/LLN0.dsEvents",
        .signal_count = 1U,
        .signals = signals,
    };
    reports[0] = (UnitLabIedFixtureReport){
        .key = "IED1/AP1/LD0/LLN0/brcbEvents/buffered",
        .logical_device_inst = "LD0",
        .logical_node_name = "LLN0",
        .report_control_name = "brcbEvents",
        .report_kind = "buffered",
        .rpt_id = "events",
        .data_set_ref = "IED1/AP1/LD0/LLN0.dsEvents",
        .conf_rev = "1",
        .indexed_known = 1,
        .indexed = 0,
        .buffer_time_ms_known = 1,
        .buffer_time_ms = 25,
        .integrity_period_ms_known = 1,
        .integrity_period_ms = 1000,
        .trigger_options = {
            .data_change = { .known = 1, .value = 1 },
            .quality_change = { .known = 1, .value = 1 },
            .data_update = { .known = 1, .value = 0 },
            .periodic = { .known = 1, .value = 0 },
            .general_interrogation = { .known = 1, .value = 1 },
        },
        .optional_fields = {
            .sequence_number = { .known = 1, .value = 1 },
            .timestamp = { .known = 1, .value = 1 },
            .reason_code = { .known = 1, .value = 1 },
            .data_set_name = { .known = 1, .value = 1 },
            .data_reference = { .known = 1, .value = 1 },
            .entry_id = { .known = 1, .value = 1 },
            .config_revision = { .known = 1, .value = 1 },
            .buffer_overflow = { .known = 1, .value = 1 },
        },
    };
    return (UnitLabIedFixtureModel){
        .device_count = 1U,
        .ied_name = "IED1",
        .access_point_name = "AP1",
        .data_set_count = 1U,
        .data_sets = data_sets,
        .report_count = 1U,
        .reports = reports,
        .signal_count = 1U,
    };
}

static int stop_requested(void* context)
{
    return ((ServerThreadContext*)context)->stop_requested != 0;
}

static void* run_server_thread(void* parameter)
{
    ServerThreadContext* context = (ServerThreadContext*)parameter;
    context->run_result = unitlab_run_ied_server(
        context->fixture,
        context->plan,
        &context->config,
        stop_requested,
        context,
        &context->load_result);
    return NULL;
}

static IedConnection connect_with_retry(int port, IedClientError* final_error)
{
    for (int attempt = 0; attempt < 30; attempt++) {
        IedConnection connection = IedConnection_create();
        IedConnection_setConnectTimeout(connection, 200U);
        IedConnection_setRequestTimeout(connection, 1000U);

        IedClientError error = IED_ERROR_OK;
        IedConnection_connect(connection, &error, "127.0.0.1", port);
        if (error == IED_ERROR_OK) {
            *final_error = error;
            return connection;
        }

        *final_error = error;
        IedConnection_destroy(connection);
        Thread_sleep(100);
    }
    return NULL;
}

static int verify_server_metadata(IedConnection connection)
{
    int passed = 1;
    IedClientError error = IED_ERROR_OK;

    LinkedList devices = IedConnection_getLogicalDeviceList(connection, &error);
    passed &= expect_true(error == IED_ERROR_OK, "logical device list read should succeed");
    passed &= expect_true(devices != NULL, "logical device list should be present");
    if (devices != NULL) {
        passed &= expect_true(string_list_contains(devices, "IED1LD0"), "server should expose IED-prefixed LD");
        LinkedList_destroy(devices);
    }

    LinkedList data_sets = IedConnection_getLogicalNodeDirectory(connection, &error, "IED1LD0/LLN0", ACSI_CLASS_DATA_SET);
    passed &= expect_true(error == IED_ERROR_OK, "DataSet directory read should succeed");
    passed &= expect_true(data_sets != NULL, "DataSet directory should be present");
    if (data_sets != NULL) {
        passed &= expect_true(string_list_contains(data_sets, "dsEvents"), "LLN0 should expose the fixture DataSet");
        LinkedList_destroy(data_sets);
    }

    bool is_deletable = true;
    LinkedList data_set_members = IedConnection_getDataSetDirectory(connection, &error, "IED1LD0/LLN0.dsEvents", &is_deletable);
    passed &= expect_true(error == IED_ERROR_OK, "DataSet member directory read should succeed");
    passed &= expect_true(data_set_members != NULL, "DataSet member directory should be present");
    passed &= expect_true(!is_deletable, "fixture DataSet should be non-deletable");
    if (data_set_members != NULL) {
        passed &= expect_true(LinkedList_size(data_set_members) == 1, "DataSet should expose one member");
        LinkedList_destroy(data_set_members);
    }

    LinkedList reports = IedConnection_getLogicalNodeDirectory(connection, &error, "IED1LD0/LLN0", ACSI_CLASS_BRCB);
    passed &= expect_true(error == IED_ERROR_OK, "BRCB directory read should succeed");
    passed &= expect_true(reports != NULL, "BRCB directory should be present");
    if (reports != NULL) {
        passed &= expect_true(string_list_contains(reports, "brcbEvents"), "LLN0 should expose the fixture BRCB");
        LinkedList_destroy(reports);
    }

    ClientReportControlBlock rcb = IedConnection_getRCBValues(connection, &error, "IED1LD0/LLN0.BR.brcbEvents", NULL);
    passed &= expect_true(error == IED_ERROR_OK, "BRCB metadata read should succeed");
    passed &= expect_true(rcb != NULL, "BRCB metadata should be present");
    if (rcb != NULL) {
        passed &= expect_string(ClientReportControlBlock_getRptId(rcb), "events", "RptID should come from fixture");
        passed &= expect_true(ClientReportControlBlock_isBuffered(rcb), "BRCB should be buffered");
        passed &= expect_true(ClientReportControlBlock_getConfRev(rcb) == 1U, "ConfRev should come from fixture");
        passed &= expect_true(ClientReportControlBlock_getBufTm(rcb) == 25U, "BufTm should come from fixture");
        passed &= expect_true(ClientReportControlBlock_getIntgPd(rcb) == 1000U, "IntgPd should come from fixture");
        passed &= expect_string_contains(ClientReportControlBlock_getDataSetReference(rcb), "dsEvents", "DatSet should point to fixture DataSet");
        ClientReportControlBlock_destroy(rcb);
    }

    return passed;
}

int main(void)
{
    UnitLabIedFixtureSignal signals[1];
    UnitLabIedFixtureDataSet data_sets[1];
    UnitLabIedFixtureReport reports[1];
    UnitLabIedFixtureModel fixture = valid_fixture(data_sets, reports, signals);
    UnitLabIedModelPlan plan;
    char error[256];

    if (!unitlab_build_ied_model_plan(&fixture, &plan, error, sizeof(error))) {
        fprintf(stderr, "FAIL: model plan should build: %s\n", error);
        return 1;
    }

    ServerThreadContext server = {
        .fixture = &fixture,
        .plan = &plan,
        .config = {
            .bind_address = "127.0.0.1",
            .port = 15120,
        },
        .stop_requested = 0,
        .run_result = 0,
    };

    Thread server_thread = Thread_create(run_server_thread, &server, false);
    if (server_thread == NULL) {
        fprintf(stderr, "FAIL: server thread should be created\n");
        unitlab_free_ied_model_plan(&plan);
        return 1;
    }
    Thread_start(server_thread);

    IedClientError connect_error = IED_ERROR_OK;
    IedConnection connection = connect_with_retry(server.config.port, &connect_error);
    int passed = expect_true(connection != NULL, IedClientError_toString(connect_error));

    if (connection != NULL) {
        passed &= verify_server_metadata(connection);
        IedConnection_close(connection);
        IedConnection_destroy(connection);
    }

    UnitLabIedModelLoadResult probe_result;
    passed &= expect_true(
        unitlab_probe_ied_server_metadata(&fixture, &plan, &server.config, &probe_result),
        probe_result.message);
    passed &= expect_string(probe_result.code, "IEC61850_METADATA_PROBE_OK", "metadata probe status code");

    server.stop_requested = 1;
    Thread_destroy(server_thread);
    passed &= expect_true(server.run_result, "linked server should stop cleanly");
    passed &= expect_string(server.load_result.code, "LIBIEC61850_SERVER_STOPPED", "server final status code");

    unitlab_free_ied_model_plan(&plan);
    return passed ? 0 : 1;
}
