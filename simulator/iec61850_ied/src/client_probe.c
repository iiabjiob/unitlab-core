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

#include <hal_thread.h>
#include <iec61850_client.h>

#include <stdlib.h>

typedef struct UnitLabGiProbeContext {
    volatile int report_count;
    const UnitLabIedModelPlan* plan;
    const UnitLabIedModelDataSet* data_set;
    int value_count;
    int first_reason;
    int conf_rev;
    int value_validation_failed;
    size_t value_validation_index;
    char value_validation_code[96];
    char value_validation_message[256];
    char rpt_id[128];
    char data_set_name[256];
} UnitLabGiProbeContext;

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

static int parse_int32_value(const char* source, int32_t* value)
{
    char* end = NULL;
    long parsed = strtol(source, &end, 10);
    if (source == end || end == NULL || *end != '\0' || parsed < INT32_MIN || parsed > INT32_MAX) {
        return 0;
    }
    *value = (int32_t)parsed;
    return 1;
}

static int parse_real32_value(const char* source, float* value)
{
    char* end = NULL;
    double parsed = strtod(source, &end);
    if (source == end || end == NULL || *end != '\0') {
        return 0;
    }
    *value = (float)parsed;
    return 1;
}

static void set_value_validation_error(UnitLabGiProbeContext* context, size_t index, const char* code, const char* message)
{
    if (context->value_validation_failed) {
        return;
    }
    context->value_validation_failed = 1;
    context->value_validation_index = index;
    snprintf(context->value_validation_code, sizeof(context->value_validation_code), "%s", code);
    snprintf(context->value_validation_message, sizeof(context->value_validation_message), "%s", message);
}

static int validate_gi_mms_value(
    const UnitLabIedModelSignal* signal,
    MmsValue* value,
    char* code,
    size_t code_size,
    char* message,
    size_t message_size)
{
    switch (signal->initial_value_kind) {
        case UNITLAB_IED_FIXTURE_VALUE_BOOLEAN: {
            int expected = 0;
            if (strcmp(signal->initial_value, "true") == 0) {
                expected = 1;
            }
            else if (strcmp(signal->initial_value, "false") != 0) {
                snprintf(code, code_size, "IEC61850_GI_PROBE_VALUE_PARSE_FAILED");
                snprintf(message, message_size, "IEC 61850 GI probe could not parse an expected boolean value.");
                return 0;
            }
            if ((MmsValue_getBoolean(value) ? 1 : 0) != expected) {
                snprintf(code, code_size, "IEC61850_GI_PROBE_VALUE_MISMATCH");
                snprintf(message, message_size, "IEC 61850 GI probe read an unexpected boolean value.");
                return 0;
            }
            return 1;
        }
        case UNITLAB_IED_FIXTURE_VALUE_INTEGER: {
            int32_t expected = 0;
            if (!parse_int32_value(signal->initial_value, &expected)) {
                snprintf(code, code_size, "IEC61850_GI_PROBE_VALUE_PARSE_FAILED");
                snprintf(message, message_size, "IEC 61850 GI probe could not parse an expected integer value.");
                return 0;
            }
            if (MmsValue_toInt32(value) != expected) {
                snprintf(code, code_size, "IEC61850_GI_PROBE_VALUE_MISMATCH");
                snprintf(message, message_size, "IEC 61850 GI probe read an unexpected integer value.");
                return 0;
            }
            return 1;
        }
        case UNITLAB_IED_FIXTURE_VALUE_REAL: {
            float expected = 0.0F;
            float actual = MmsValue_toFloat(value);
            float delta = actual;
            if (!parse_real32_value(signal->initial_value, &expected)) {
                snprintf(code, code_size, "IEC61850_GI_PROBE_VALUE_PARSE_FAILED");
                snprintf(message, message_size, "IEC 61850 GI probe could not parse an expected real value.");
                return 0;
            }
            delta -= expected;
            if (delta < 0.0F) {
                delta = -delta;
            }
            if (delta > 0.0001F) {
                snprintf(code, code_size, "IEC61850_GI_PROBE_VALUE_MISMATCH");
                snprintf(message, message_size, "IEC 61850 GI probe read an unexpected real value.");
                return 0;
            }
            return 1;
        }
        case UNITLAB_IED_FIXTURE_VALUE_STRING: {
            const char* string_value = MmsValue_toString(value);
            if (string_value == NULL || strcmp(string_value, signal->initial_value) != 0) {
                snprintf(code, code_size, "IEC61850_GI_PROBE_VALUE_MISMATCH");
                snprintf(message, message_size, "IEC 61850 GI probe read an unexpected string value.");
                return 0;
            }
            return 1;
        }
        case UNITLAB_IED_FIXTURE_VALUE_NULL:
        case UNITLAB_IED_FIXTURE_VALUE_UNKNOWN:
        default:
            snprintf(code, code_size, "IEC61850_GI_PROBE_VALUE_KIND_UNSUPPORTED");
            snprintf(message, message_size, "IEC 61850 GI probe requires typed DataSet values.");
            return 0;
    }
}

static void report_callback(void* parameter, ClientReport report)
{
    UnitLabGiProbeContext* context = (UnitLabGiProbeContext*)parameter;
    context->report_count++;

    char* rpt_id = ClientReport_getRptId(report);
    if (rpt_id != NULL) {
        snprintf(context->rpt_id, sizeof(context->rpt_id), "%s", rpt_id);
    }

    const char* data_set_name = ClientReport_getDataSetName(report);
    if (data_set_name != NULL) {
        snprintf(context->data_set_name, sizeof(context->data_set_name), "%s", data_set_name);
    }

    if (ClientReport_hasConfRev(report)) {
        context->conf_rev = (int)ClientReport_getConfRev(report);
    }
    if (ClientReport_hasReasonForInclusion(report)) {
        context->first_reason = ClientReport_getReasonForInclusion(report, 0);
    }

    MmsValue* values = ClientReport_getDataSetValues(report);
    if (values == NULL) {
        return;
    }
    context->value_count = (int)MmsValue_getArraySize(values);
    if (context->value_count == 0) {
        return;
    }

    if (context->plan == NULL || context->data_set == NULL) {
        set_value_validation_error(context, 0U, "IEC61850_GI_PROBE_CONTEXT_INVALID", "IEC 61850 GI probe callback is missing DataSet context.");
        return;
    }
    for (size_t index = 0U; index < context->data_set->member_count; index++) {
        size_t signal_index = context->data_set->first_signal_index + index;
        if (signal_index >= context->plan->signal_count) {
            set_value_validation_error(context, index, "IEC61850_GI_PROBE_SIGNAL_INDEX_INVALID", "IEC 61850 GI probe DataSet member points outside the signal plan.");
            return;
        }
        if (index >= (size_t)context->value_count) {
            set_value_validation_error(context, index, "IEC61850_GI_PROBE_VALUE_COUNT_MISMATCH", "IEC 61850 GI probe received fewer values than expected.");
            return;
        }
        MmsValue* value = MmsValue_getElement(values, index);
        if (value == NULL) {
            set_value_validation_error(context, index, "IEC61850_GI_PROBE_VALUE_MISSING", "IEC 61850 GI probe received a null DataSet value.");
            return;
        }
        char code[96];
        char message[256];
        if (!validate_gi_mms_value(&context->plan->signals[signal_index], value, code, sizeof(code), message, sizeof(message))) {
            set_value_validation_error(context, index, code, message);
            return;
        }
    }
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

static int probe_gi_report(
    IedConnection connection,
    const UnitLabIedFixtureModel* fixture,
    const UnitLabIedModelPlan* plan,
    const UnitLabIedModelReportControl* report,
    UnitLabIedModelLoadResult* result)
{
    if (report->data_set_index >= plan->data_set_count) {
        set_probe_result(result, 0, "IEC61850_GI_PROBE_DATASET_INDEX_INVALID", "IEC 61850 GI probe found a ReportControl with an invalid DataSet index.");
        return 0;
    }

    const UnitLabIedModelDataSet* data_set = &plan->data_sets[report->data_set_index];
    if (data_set->member_count == 0U || data_set->first_signal_index >= plan->signal_count) {
        set_probe_result(result, 0, "IEC61850_GI_PROBE_DATASET_EMPTY", "IEC 61850 GI probe requires every ReportControl DataSet to have at least one signal.");
        return 0;
    }

    char logical_node_ref[256];
    char rcb_ref[384];
    int passed = format_ref(
        logical_node_ref,
        sizeof(logical_node_ref),
        result,
        "IEC61850_GI_PROBE_LN_REF_OVERFLOW",
        "%s%s/%s",
        fixture->ied_name,
        report->logical_device_inst,
        report->logical_node_name);
    if (passed) {
        passed = format_ref(
            rcb_ref,
            sizeof(rcb_ref),
            result,
            "IEC61850_GI_PROBE_RCB_REF_OVERFLOW",
            report->is_buffered ? "%s.BR.%s" : "%s.RP.%s",
            logical_node_ref,
            report->name,
            "");
    }

    IedClientError error = IED_ERROR_OK;
    ClientReportControlBlock rcb = NULL;
    UnitLabGiProbeContext context = {
        .plan = plan,
        .data_set = data_set,
    };
    if (passed) {
        rcb = IedConnection_getRCBValues(connection, &error, rcb_ref, NULL);
        if (error != IED_ERROR_OK || rcb == NULL) {
            set_probe_result(result, 0, "IEC61850_GI_PROBE_RCB_READ_FAILED", "IEC 61850 GI probe failed to read ReportControl values.");
            passed = 0;
        }
    }

    if (passed) {
        IedConnection_installReportHandler(connection, rcb_ref, ClientReportControlBlock_getRptId(rcb), report_callback, &context);
        if (report->is_buffered) {
            ClientReportControlBlock_setResvTms(rcb, 30);
            IedConnection_setRCBValues(connection, &error, rcb, RCB_ELEMENT_RESV_TMS, true);
            if (error != IED_ERROR_OK) {
                char message[256];
                snprintf(message, sizeof(message), "IEC 61850 GI probe failed to reserve the buffered ReportControl: %s.", IedClientError_toString(error));
                set_probe_result(result, 0, "IEC61850_GI_PROBE_RESERVE_FAILED", message);
                passed = 0;
            }
        }
    }

    if (passed) {
        ClientReportControlBlock_setRptEna(rcb, true);
        IedConnection_setRCBValues(connection, &error, rcb, RCB_ELEMENT_RPT_ENA, true);
        if (error != IED_ERROR_OK) {
            char message[256];
            snprintf(message, sizeof(message), "IEC 61850 GI probe failed to enable the ReportControl: %s.", IedClientError_toString(error));
            set_probe_result(result, 0, "IEC61850_GI_PROBE_ENABLE_FAILED", message);
            passed = 0;
        }
    }

    if (passed) {
        ClientReportControlBlock_setGI(rcb, true);
        IedConnection_setRCBValues(connection, &error, rcb, RCB_ELEMENT_GI, true);
        if (error != IED_ERROR_OK) {
            char message[256];
            snprintf(message, sizeof(message), "IEC 61850 GI probe failed to request GI: %s.", IedClientError_toString(error));
            set_probe_result(result, 0, "IEC61850_GI_PROBE_GI_FAILED", message);
            passed = 0;
        }
    }

    if (passed) {
        for (int attempt = 0; attempt < 30 && context.report_count == 0; attempt++) {
            Thread_sleep(100);
        }
        if (context.report_count == 0) {
            set_probe_result(result, 0, "IEC61850_GI_PROBE_REPORT_TIMEOUT", "IEC 61850 GI probe did not receive a report after GI.");
            passed = 0;
        }
    }

    if (passed && report->rpt_id[0] != '\0' && strcmp(context.rpt_id, report->rpt_id) != 0) {
        set_probe_result(result, 0, "IEC61850_GI_PROBE_RPTID_MISMATCH", "IEC 61850 GI probe received an unexpected RptID.");
        passed = 0;
    }
    if (passed && strstr(context.data_set_name, data_set->name) == NULL) {
        set_probe_result(result, 0, "IEC61850_GI_PROBE_DATASET_MISMATCH", "IEC 61850 GI probe received an unexpected DataSet name.");
        passed = 0;
    }
    if (passed && report->conf_rev_known && context.conf_rev != (int)report->conf_rev) {
        set_probe_result(result, 0, "IEC61850_GI_PROBE_CONFREV_MISMATCH", "IEC 61850 GI probe received an unexpected ConfRev.");
        passed = 0;
    }
    if (passed && context.value_count != (int)data_set->member_count) {
        set_probe_result(result, 0, "IEC61850_GI_PROBE_VALUE_COUNT_MISMATCH", "IEC 61850 GI probe received an unexpected number of DataSet values.");
        passed = 0;
    }
    if (passed && context.value_validation_failed) {
        char message[320];
        snprintf(message, sizeof(message), "%s DataSet member index: %zu.", context.value_validation_message, context.value_validation_index);
        set_probe_result(result, 0, context.value_validation_code, message);
        passed = 0;
    }
    if (passed && report->optional_fields.reason_code.known && report->optional_fields.reason_code.value && (context.first_reason & IEC61850_REASON_GI) == 0) {
        set_probe_result(result, 0, "IEC61850_GI_PROBE_REASON_MISMATCH", "IEC 61850 GI probe received a report without GI reason.");
        passed = 0;
    }

    if (rcb != NULL) {
        ClientReportControlBlock_setRptEna(rcb, false);
        IedConnection_setRCBValues(connection, &error, rcb, RCB_ELEMENT_RPT_ENA, true);
        IedConnection_uninstallReportHandler(connection, rcb_ref);
        ClientReportControlBlock_destroy(rcb);
    }
    return passed;
}

int unitlab_probe_ied_server_gi(
    const UnitLabIedFixtureModel* fixture,
    const UnitLabIedModelPlan* plan,
    const UnitLabIedServerConfig* config,
    UnitLabIedModelLoadResult* result)
{
    if (fixture == NULL || plan == NULL || config == NULL || result == NULL) {
        set_probe_result(result, 0, "IEC61850_GI_PROBE_INVALID_ARGUMENT", "Fixture, model plan, server config, and result are required.");
        return 0;
    }
    if (plan->report_count == 0U || plan->data_set_count == 0U || plan->signal_count == 0U) {
        set_probe_result(result, 0, "IEC61850_GI_PROBE_EMPTY_PLAN", "IEC 61850 GI probe requires at least one report, DataSet, and signal.");
        return 0;
    }

    IedConnection connection = connect_to_server(config, result);
    if (connection == NULL) {
        return 0;
    }

    int passed = 1;
    for (size_t index = 0U; index < plan->report_count; index++) {
        if (!probe_gi_report(connection, fixture, plan, &plan->reports[index], result)) {
            passed = 0;
            break;
        }
    }

    IedConnection_close(connection);
    IedConnection_destroy(connection);

    if (!passed) {
        return 0;
    }

    char message[192];
    snprintf(message, sizeof(message), "IEC 61850 GI probe validated %zu ReportControl(s).", plan->report_count);
    set_probe_result(result, 1, "IEC61850_GI_PROBE_OK", message);
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

int unitlab_probe_ied_server_gi(
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
        "libIEC61850 is not linked; build with UNITLAB_IEC61850_SIM_WITH_LIBIEC61850=ON before probing MMS GI.");
    return 0;
}

#endif
