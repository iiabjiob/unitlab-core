#include "client_probe.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static void set_probe_result(UnitLabIedModelLoadResult* result, int loaded, const char* code, const char* message)
{
    if (result == NULL) {
        return;
    }
    result->loaded = loaded;
    snprintf(result->code, sizeof(result->code), "%s", code);
    snprintf(result->message, sizeof(result->message), "%s", message);
}

#ifdef UNITLAB_WITH_LIBIEC61850

#include <iec61850_client.h>

static int list_contains(LinkedList list, const char* expected)
{
    for (LinkedList entry = LinkedList_getNext(list); entry != NULL; entry = LinkedList_getNext(entry)) {
        const char* value = (const char*)LinkedList_getData(entry);
        if (value != NULL && strcmp(value, expected) == 0) {
            return 1;
        }
    }
    return 0;
}

static int format_ref(char* buffer, size_t buffer_size, UnitLabIedModelLoadResult* result, const char* code, const char* format, const char* first, const char* second, const char* third)
{
    int written = snprintf(buffer, buffer_size, format, first, second, third);
    if (written < 0 || (size_t)written >= buffer_size) {
        set_probe_result(result, 0, code, "IEC 61850 metadata probe reference buffer is too small.");
        return 0;
    }
    return 1;
}

static const char* connect_host(const UnitLabIedServerConfig* config)
{
    if (strcmp(config->bind_address, "0.0.0.0") == 0) {
        return "127.0.0.1";
    }
    return config->bind_address;
}

static IedConnection connect_to_server(const UnitLabIedServerConfig* config, UnitLabIedModelLoadResult* result)
{
    IedConnection connection = IedConnection_create();
    if (connection == NULL) {
        set_probe_result(result, 0, "IEC61850_METADATA_PROBE_CONNECTION_CREATE_FAILED", "libIEC61850 failed to create a client connection.");
        return NULL;
    }

    IedConnection_setConnectTimeout(connection, 1000U);
    IedConnection_setRequestTimeout(connection, 2000U);

    IedClientError error = IED_ERROR_OK;
    IedConnection_connect(connection, &error, connect_host(config), config->port);
    if (error != IED_ERROR_OK) {
        char message[256];
        snprintf(message, sizeof(message), "IEC 61850 metadata probe failed to connect: %s.", IedClientError_toString(error));
        set_probe_result(result, 0, "IEC61850_METADATA_PROBE_CONNECT_FAILED", message);
        IedConnection_destroy(connection);
        return NULL;
    }

    return connection;
}

static int verify_logical_devices(
    IedConnection connection,
    const UnitLabIedFixtureModel* fixture,
    const UnitLabIedModelPlan* plan,
    UnitLabIedModelLoadResult* result)
{
    IedClientError error = IED_ERROR_OK;
    LinkedList devices = IedConnection_getLogicalDeviceList(connection, &error);
    if (error != IED_ERROR_OK || devices == NULL) {
        set_probe_result(result, 0, "IEC61850_METADATA_PROBE_LD_DIRECTORY_FAILED", "IEC 61850 metadata probe failed to read logical device directory.");
        return 0;
    }

    int passed = 1;
    for (size_t index = 0U; index < plan->logical_device_count; index++) {
        char logical_device_ref[256];
        if (!format_ref(
                logical_device_ref,
                sizeof(logical_device_ref),
                result,
                "IEC61850_METADATA_PROBE_LD_REF_OVERFLOW",
                "%s%s",
                fixture->ied_name,
                plan->logical_devices[index].inst,
                "")) {
            passed = 0;
            break;
        }
        if (!list_contains(devices, logical_device_ref)) {
            set_probe_result(result, 0, "IEC61850_METADATA_PROBE_LD_MISSING", "IEC 61850 metadata probe did not find an expected logical device.");
            passed = 0;
            break;
        }
    }

    LinkedList_destroy(devices);
    return passed;
}

static int verify_data_sets(
    IedConnection connection,
    const UnitLabIedFixtureModel* fixture,
    const UnitLabIedModelPlan* plan,
    UnitLabIedModelLoadResult* result)
{
    for (size_t index = 0U; index < plan->data_set_count; index++) {
        const UnitLabIedModelDataSet* data_set = &plan->data_sets[index];
        char logical_node_ref[256];
        if (!format_ref(
                logical_node_ref,
                sizeof(logical_node_ref),
                result,
                "IEC61850_METADATA_PROBE_LN_REF_OVERFLOW",
                "%s%s/%s",
                fixture->ied_name,
                data_set->logical_device_inst,
                data_set->logical_node_name)) {
            return 0;
        }

        IedClientError error = IED_ERROR_OK;
        LinkedList data_sets = IedConnection_getLogicalNodeDirectory(connection, &error, logical_node_ref, ACSI_CLASS_DATA_SET);
        if (error != IED_ERROR_OK || data_sets == NULL) {
            set_probe_result(result, 0, "IEC61850_METADATA_PROBE_DATASET_DIRECTORY_FAILED", "IEC 61850 metadata probe failed to read DataSet directory.");
            return 0;
        }
        if (!list_contains(data_sets, data_set->name)) {
            set_probe_result(result, 0, "IEC61850_METADATA_PROBE_DATASET_MISSING", "IEC 61850 metadata probe did not find an expected DataSet.");
            LinkedList_destroy(data_sets);
            return 0;
        }
        LinkedList_destroy(data_sets);

        char data_set_ref[384];
        if (!format_ref(
                data_set_ref,
                sizeof(data_set_ref),
                result,
                "IEC61850_METADATA_PROBE_DATASET_REF_OVERFLOW",
                "%s.%s",
                logical_node_ref,
                data_set->name,
                "")) {
            return 0;
        }

        bool is_deletable = true;
        LinkedList members = IedConnection_getDataSetDirectory(connection, &error, data_set_ref, &is_deletable);
        if (error != IED_ERROR_OK || members == NULL) {
            set_probe_result(result, 0, "IEC61850_METADATA_PROBE_DATASET_MEMBERS_FAILED", "IEC 61850 metadata probe failed to read DataSet member directory.");
            return 0;
        }

        int member_count = LinkedList_size(members);
        LinkedList_destroy(members);
        if (member_count != (int)data_set->member_count) {
            char message[256];
            snprintf(message, sizeof(message), "IEC 61850 metadata probe found %d DataSet members; expected %zu.", member_count, data_set->member_count);
            set_probe_result(result, 0, "IEC61850_METADATA_PROBE_DATASET_MEMBER_COUNT_MISMATCH", message);
            return 0;
        }
        if (is_deletable) {
            set_probe_result(result, 0, "IEC61850_METADATA_PROBE_DATASET_DELETABLE", "IEC 61850 metadata probe expected a non-deletable configured DataSet.");
            return 0;
        }
    }
    return 1;
}

static int verify_reports(
    IedConnection connection,
    const UnitLabIedFixtureModel* fixture,
    const UnitLabIedModelPlan* plan,
    UnitLabIedModelLoadResult* result)
{
    for (size_t index = 0U; index < plan->report_count; index++) {
        const UnitLabIedModelReportControl* report = &plan->reports[index];
        char logical_node_ref[256];
        if (!format_ref(
                logical_node_ref,
                sizeof(logical_node_ref),
                result,
                "IEC61850_METADATA_PROBE_LN_REF_OVERFLOW",
                "%s%s/%s",
                fixture->ied_name,
                report->logical_device_inst,
                report->logical_node_name)) {
            return 0;
        }

        IedClientError error = IED_ERROR_OK;
        ACSIClass report_class = report->is_buffered ? ACSI_CLASS_BRCB : ACSI_CLASS_URCB;
        LinkedList reports = IedConnection_getLogicalNodeDirectory(connection, &error, logical_node_ref, report_class);
        if (error != IED_ERROR_OK || reports == NULL) {
            set_probe_result(result, 0, "IEC61850_METADATA_PROBE_RCB_DIRECTORY_FAILED", "IEC 61850 metadata probe failed to read ReportControl directory.");
            return 0;
        }
        if (!list_contains(reports, report->name)) {
            set_probe_result(result, 0, "IEC61850_METADATA_PROBE_RCB_MISSING", "IEC 61850 metadata probe did not find an expected ReportControl.");
            LinkedList_destroy(reports);
            return 0;
        }
        LinkedList_destroy(reports);

        char rcb_ref[384];
        if (!format_ref(
                rcb_ref,
                sizeof(rcb_ref),
                result,
                "IEC61850_METADATA_PROBE_RCB_REF_OVERFLOW",
                report->is_buffered ? "%s.BR.%s" : "%s.RP.%s",
                logical_node_ref,
                report->name,
                "")) {
            return 0;
        }

        ClientReportControlBlock rcb = IedConnection_getRCBValues(connection, &error, rcb_ref, NULL);
        if (error != IED_ERROR_OK || rcb == NULL) {
            set_probe_result(result, 0, "IEC61850_METADATA_PROBE_RCB_READ_FAILED", "IEC 61850 metadata probe failed to read ReportControl values.");
            return 0;
        }

        int passed = 1;
        const char* rpt_id = ClientReportControlBlock_getRptId(rcb);
        if (report->rpt_id[0] != '\0' && (rpt_id == NULL || strcmp(rpt_id, report->rpt_id) != 0)) {
            set_probe_result(result, 0, "IEC61850_METADATA_PROBE_RPTID_MISMATCH", "IEC 61850 metadata probe read an unexpected RptID.");
            passed = 0;
        }
        if (passed && ClientReportControlBlock_isBuffered(rcb) != (report->is_buffered ? true : false)) {
            set_probe_result(result, 0, "IEC61850_METADATA_PROBE_RCB_KIND_MISMATCH", "IEC 61850 metadata probe read an unexpected ReportControl kind.");
            passed = 0;
        }
        if (passed && report->conf_rev_known && ClientReportControlBlock_getConfRev(rcb) != report->conf_rev) {
            set_probe_result(result, 0, "IEC61850_METADATA_PROBE_CONFREV_MISMATCH", "IEC 61850 metadata probe read an unexpected ConfRev.");
            passed = 0;
        }
        if (passed && report->buffer_time_ms_known && ClientReportControlBlock_getBufTm(rcb) != report->buffer_time_ms) {
            set_probe_result(result, 0, "IEC61850_METADATA_PROBE_BUFTM_MISMATCH", "IEC 61850 metadata probe read an unexpected BufTm.");
            passed = 0;
        }
        if (passed && report->integrity_period_ms_known && ClientReportControlBlock_getIntgPd(rcb) != report->integrity_period_ms) {
            set_probe_result(result, 0, "IEC61850_METADATA_PROBE_INTGPD_MISMATCH", "IEC 61850 metadata probe read an unexpected IntgPd.");
            passed = 0;
        }
        const UnitLabIedModelDataSet* data_set = &plan->data_sets[report->data_set_index];
        const char* data_set_ref = ClientReportControlBlock_getDataSetReference(rcb);
        if (passed && (data_set_ref == NULL || strstr(data_set_ref, data_set->name) == NULL)) {
            set_probe_result(result, 0, "IEC61850_METADATA_PROBE_DATASET_REF_MISMATCH", "IEC 61850 metadata probe read an unexpected DatSet value.");
            passed = 0;
        }

        ClientReportControlBlock_destroy(rcb);
        if (!passed) {
            return 0;
        }
    }
    return 1;
}

int unitlab_probe_ied_server_metadata(
    const UnitLabIedFixtureModel* fixture,
    const UnitLabIedModelPlan* plan,
    const UnitLabIedServerConfig* config,
    UnitLabIedModelLoadResult* result)
{
    if (fixture == NULL || plan == NULL || config == NULL || result == NULL) {
        set_probe_result(result, 0, "IEC61850_METADATA_PROBE_INVALID_ARGUMENT", "Fixture, model plan, server config, and result are required.");
        return 0;
    }

    IedConnection connection = connect_to_server(config, result);
    if (connection == NULL) {
        return 0;
    }

    int passed = verify_logical_devices(connection, fixture, plan, result)
        && verify_data_sets(connection, fixture, plan, result)
        && verify_reports(connection, fixture, plan, result);

    IedConnection_close(connection);
    IedConnection_destroy(connection);
    if (!passed) {
        return 0;
    }

    set_probe_result(result, 1, "IEC61850_METADATA_PROBE_OK", "IEC 61850 metadata probe read DataSet and ReportControl metadata successfully.");
    return 1;
}

#else

int unitlab_probe_ied_server_metadata(
    const UnitLabIedFixtureModel* fixture,
    const UnitLabIedModelPlan* plan,
    const UnitLabIedServerConfig* config,
    UnitLabIedModelLoadResult* result)
{
    (void)fixture;
    (void)plan;
    (void)config;
    set_probe_result(
        result,
        0,
        "LIBIEC61850_NOT_LINKED",
        "libIEC61850 is not linked; build with UNITLAB_IEC61850_SIM_WITH_LIBIEC61850=ON before probing MMS metadata.");
    return 0;
}

#endif
