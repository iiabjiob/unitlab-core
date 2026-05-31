#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "client_probe.h"
#include "fixture_parser.h"
#include "model_loader.h"
#include "model_plan.h"
#include "unitlab_mms_server_runtime.h"
#include "wire/ber/unitlab_mms_ber.h"
#include "wire/mms/unitlab_mms_pdu.h"
#include "wire/transport/unitlab_mms_wire_association_fixture.h"

typedef struct SimulatorOptions {
    const char* fixture_path;
    const char* ied_name;
    const char* bind_address;
    int port;
    int dry_run;
    int smoke_start;
    int native_smoke_start;
    int metadata_probe;
    int gi_probe;
    const char* report_key;
} SimulatorOptions;

static volatile sig_atomic_t g_running = 1;

static void handle_stop_signal(int signal_number)
{
    (void)signal_number;
    g_running = 0;
}

static int signal_stop_requested(void* context)
{
    (void)context;
    return g_running == 0;
}

static int immediate_stop_requested(void* context)
{
    (void)context;
    return 1;
}

static void print_usage(const char* program_name)
{
    printf("Usage: %s --fixture PATH --ied NAME [--bind ADDRESS] [--port PORT] [--dry-run] [--smoke-start] [--native-smoke-start] [--metadata-probe] [--gi-probe] [--report-key KEY]\n", program_name);
    printf("\n");
    printf("Options:\n");
    printf("  --fixture PATH   UnitLab IEC 61850 IED simulator fixture JSON.\n");
    printf("  --ied NAME       IED name from the fixture to expose.\n");
    printf("  --bind ADDRESS   Bind address for the MMS server. Default: 0.0.0.0.\n");
    printf("  --port PORT      TCP port for the MMS server. Default: 102.\n");
    printf("  --dry-run        Validate CLI and fixture boundary without opening MMS.\n");
    printf("  --smoke-start    Start and stop the linked MMS server once, then exit.\n");
    printf("  --native-smoke-start Exercise the native server-runtime boundary once, then exit.\n");
    printf("  --metadata-probe Connect to the endpoint and verify DataSet/BRCB metadata, then exit.\n");
    printf("  --gi-probe       Connect to the endpoint, enable report(s), request GI, verify fixture values, then exit.\n");
    printf("  --report-key KEY Limit --gi-probe validation to one fixture ReportControl key.\n");
    printf("  --help           Show this help text.\n");
}

static int parse_int(const char* value, int* out)
{
    char* end = NULL;
    long parsed = strtol(value, &end, 10);
    if (value == end || end == NULL || *end != '\0') {
        return 0;
    }
    if (parsed <= 0 || parsed > 65535) {
        return 0;
    }
    *out = (int)parsed;
    return 1;
}

static int parse_args(int argc, char** argv, SimulatorOptions* options)
{
    options->fixture_path = NULL;
    options->ied_name = NULL;
    options->bind_address = "0.0.0.0";
    options->port = 102;
    options->dry_run = 0;
    options->smoke_start = 0;
    options->native_smoke_start = 0;
    options->metadata_probe = 0;
    options->gi_probe = 0;
    options->report_key = NULL;

    for (int index = 1; index < argc; index++) {
        const char* arg = argv[index];
        if (strcmp(arg, "--help") == 0) {
            print_usage(argv[0]);
            return 1;
        }
        if (strcmp(arg, "--dry-run") == 0) {
            options->dry_run = 1;
            continue;
        }
        if (strcmp(arg, "--smoke-start") == 0) {
            options->smoke_start = 1;
            continue;
        }
        if (strcmp(arg, "--native-smoke-start") == 0) {
            options->native_smoke_start = 1;
            continue;
        }
        if (strcmp(arg, "--metadata-probe") == 0) {
            options->metadata_probe = 1;
            continue;
        }
        if (strcmp(arg, "--gi-probe") == 0) {
            options->gi_probe = 1;
            continue;
        }
        if (strcmp(arg, "--report-key") == 0 && index + 1 < argc) {
            options->report_key = argv[++index];
            continue;
        }
        if (strcmp(arg, "--fixture") == 0 && index + 1 < argc) {
            options->fixture_path = argv[++index];
            continue;
        }
        if (strcmp(arg, "--ied") == 0 && index + 1 < argc) {
            options->ied_name = argv[++index];
            continue;
        }
        if (strcmp(arg, "--bind") == 0 && index + 1 < argc) {
            options->bind_address = argv[++index];
            continue;
        }
        if (strcmp(arg, "--port") == 0 && index + 1 < argc) {
            if (!parse_int(argv[++index], &options->port)) {
                fprintf(stderr, "INVALID_PORT: expected TCP port in range 1..65535.\n");
                return -1;
            }
            continue;
        }
        fprintf(stderr, "INVALID_ARGUMENT: unsupported or incomplete argument \"%s\".\n", arg);
        return -1;
    }

    if (options->fixture_path == NULL || options->fixture_path[0] == '\0') {
        fprintf(stderr, "FIXTURE_REQUIRED: --fixture PATH is required.\n");
        return -1;
    }
    if (options->ied_name == NULL || options->ied_name[0] == '\0') {
        fprintf(stderr, "IED_REQUIRED: --ied NAME is required.\n");
        return -1;
    }
    if (options->bind_address == NULL || options->bind_address[0] == '\0') {
        fprintf(stderr, "BIND_REQUIRED: --bind ADDRESS cannot be empty.\n");
        return -1;
    }
    if ((options->dry_run ? 1 : 0) + (options->smoke_start ? 1 : 0) + (options->native_smoke_start ? 1 : 0) + (options->metadata_probe ? 1 : 0) + (options->gi_probe ? 1 : 0) > 1) {
        fprintf(stderr, "INVALID_ARGUMENT: --dry-run, --smoke-start, --native-smoke-start, --metadata-probe, and --gi-probe are mutually exclusive.\n");
        return -1;
    }
    if (options->report_key != NULL && options->report_key[0] == '\0') {
        fprintf(stderr, "INVALID_ARGUMENT: --report-key cannot be empty.\n");
        return -1;
    }
    if (options->report_key != NULL && !options->gi_probe) {
        fprintf(stderr, "INVALID_ARGUMENT: --report-key requires --gi-probe.\n");
        return -1;
    }

    return 0;
}

static char* read_text_file(const char* path)
{
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "FIXTURE_OPEN_FAILED: %s: %s\n", path, strerror(errno));
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fprintf(stderr, "FIXTURE_READ_FAILED: %s: %s\n", path, strerror(errno));
        fclose(file);
        return NULL;
    }

    long length = ftell(file);
    if (length < 0) {
        fprintf(stderr, "FIXTURE_READ_FAILED: %s: %s\n", path, strerror(errno));
        fclose(file);
        return NULL;
    }
    if (length == 0) {
        fprintf(stderr, "FIXTURE_EMPTY: %s\n", path);
        fclose(file);
        return NULL;
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        fprintf(stderr, "FIXTURE_READ_FAILED: %s: %s\n", path, strerror(errno));
        fclose(file);
        return NULL;
    }

    char* buffer = (char*)malloc((size_t)length + 1U);
    if (buffer == NULL) {
        fprintf(stderr, "OUT_OF_MEMORY: cannot allocate fixture buffer.\n");
        fclose(file);
        return NULL;
    }

    size_t read_count = fread(buffer, 1U, (size_t)length, file);
    fclose(file);
    if (read_count != (size_t)length) {
        fprintf(stderr, "FIXTURE_READ_FAILED: %s\n", path);
        free(buffer);
        return NULL;
    }

    buffer[length] = '\0';
    return buffer;
}

static const char* libiec61850_status(void)
{
#ifdef UNITLAB_WITH_LIBIEC61850
    return "linked";
#else
    return "not-linked";
#endif
}

static const char* optional_bool_label(UnitLabIedFixtureOptionalBool field)
{
    if (!field.known) {
        return "unknown";
    }
    return field.value ? "true" : "false";
}

static int build_native_information_report_association_bytes(uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsWireAssociationFixture fixture;
    UnitLabMmsPdu pdu;
    UnitLabMmsBerElement service_element;
    uint8_t service_bytes[8];
    uint8_t pdu_bytes[32];
    size_t service_length = 0U;
    size_t pdu_length = 0U;

    if (buffer == NULL || encoded_length == NULL) {
        if (diagnostic != NULL) {
            diagnostic->code = UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT;
            diagnostic->message[0] = '\0';
        }
        return 0;
    }

    unitlab_mms_ber_element_init(&service_element);
    service_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    service_element.tag.constructed = 1;
    service_element.tag.tag_number = 0U;
    service_element.value_bytes = NULL;
    service_element.value_length = 0U;
    if (!unitlab_mms_ber_write(&service_element, service_bytes, sizeof(service_bytes), &service_length, diagnostic)) {
        return 0;
    }

    unitlab_mms_pdu_init(&pdu);
    pdu.kind = UNITLAB_MMS_PDU_UNCONFIRMED;
    pdu.has_service = 1;
    pdu.service_kind = UNITLAB_MMS_SERVICE_INFORMATION_REPORT;
    pdu.pdu_bytes = service_bytes;
    pdu.pdu_length = service_length;
    if (!unitlab_mms_pdu_encode(&pdu, pdu_bytes, sizeof(pdu_bytes), &pdu_length, diagnostic)) {
        return 0;
    }

    unitlab_mms_wire_association_fixture_init(&fixture);
    fixture.session.kind = UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER;
    fixture.presentation.kind = UNITLAB_MMS_PRESENTATION_APDU_SIMPLY_ENCODED;
    fixture.presentation.payload_bytes = pdu_bytes;
    fixture.presentation.payload_length = pdu_length;
    fixture.transport.cotp.kind = UNITLAB_MMS_COTP_TPDU_DT;
    fixture.transport.cotp.user_data = pdu_bytes;
    fixture.transport.cotp.user_data_length = pdu_length;
    if (!unitlab_mms_wire_association_fixture_encode(&fixture, buffer, buffer_length, encoded_length, diagnostic)) {
        return 0;
    }
    return 1;
}

static const char* value_kind_label(UnitLabIedFixtureValueKind value_kind)
{
    switch (value_kind) {
        case UNITLAB_IED_FIXTURE_VALUE_NULL:
            return "null";
        case UNITLAB_IED_FIXTURE_VALUE_BOOLEAN:
            return "boolean";
        case UNITLAB_IED_FIXTURE_VALUE_INTEGER:
            return "integer";
        case UNITLAB_IED_FIXTURE_VALUE_REAL:
            return "real";
        case UNITLAB_IED_FIXTURE_VALUE_STRING:
            return "string";
        case UNITLAB_IED_FIXTURE_VALUE_UNKNOWN:
        default:
            return "unknown";
    }
}

int main(int argc, char** argv)
{
    SimulatorOptions options;
    int parsed = parse_args(argc, argv, &options);
    if (parsed > 0) {
        return 0;
    }
    if (parsed < 0) {
        return 64;
    }

    char* fixture_text = read_text_file(options.fixture_path);
    if (fixture_text == NULL) {
        return 66;
    }

    UnitLabIedFixtureModel fixture_model;
    char fixture_error[256];
    int valid = unitlab_parse_ied_fixture_model(
        fixture_text,
        options.ied_name,
        &fixture_model,
        fixture_error,
        sizeof(fixture_error));
    free(fixture_text);
    if (!valid) {
        fprintf(stderr, "%s\n", fixture_error);
        return 65;
    }

    UnitLabIedModelPlan model_plan;
    char model_error[256];
    if (!unitlab_build_ied_model_plan(&fixture_model, &model_plan, model_error, sizeof(model_error))) {
        fprintf(stderr, "%s\n", model_error);
        unitlab_free_ied_fixture_model(&fixture_model);
        return 65;
    }

    if (options.dry_run) {
        printf("unitlab-iec61850-ied-sim: fixture accepted\n");
        printf("schema=%s\n", UNITLAB_IED_SIM_SCHEMA);
        printf("ied=%s\n", fixture_model.ied_name);
        printf("accessPoint=%s\n", fixture_model.access_point_name);
        printf("devices=%zu\n", fixture_model.device_count);
        printf("dataSets=%zu\n", fixture_model.data_set_count);
        printf("reports=%zu\n", fixture_model.report_count);
        printf("signals=%zu\n", fixture_model.signal_count);
        if (fixture_model.data_set_count > 0U) {
            printf("firstDataSet=%s\n", fixture_model.data_sets[0].reference);
        }
        if (fixture_model.data_set_count > 0U && fixture_model.data_sets[0].signal_count > 0U) {
            printf("firstSignal=%s\n", fixture_model.data_sets[0].signals[0].reference);
        }
        if (fixture_model.report_count > 0U) {
            printf("firstReport=%s\n", fixture_model.reports[0].key);
            printf("firstReportTriggerGI=%s\n", optional_bool_label(fixture_model.reports[0].trigger_options.general_interrogation));
            printf("firstReportOptDataRef=%s\n", optional_bool_label(fixture_model.reports[0].optional_fields.data_reference));
        }
        printf("modelLogicalDevices=%zu\n", model_plan.logical_device_count);
        printf("modelLogicalNodes=%zu\n", model_plan.logical_node_count);
        printf("modelDataSets=%zu\n", model_plan.data_set_count);
        printf("modelReports=%zu\n", model_plan.report_count);
        printf("modelSignals=%zu\n", model_plan.signal_count);
        if (model_plan.logical_device_count > 0U) {
            printf("firstModelLogicalDevice=%s\n", model_plan.logical_devices[0].inst);
        }
        if (model_plan.logical_node_count > 0U) {
            printf(
                "firstModelLogicalNode=%s/%s\n",
                model_plan.logical_nodes[0].logical_device_inst,
                model_plan.logical_nodes[0].name);
        }
        if (model_plan.data_set_count > 0U) {
            printf(
                "firstModelDataSet=%s/%s.%s\n",
                model_plan.data_sets[0].logical_device_inst,
                model_plan.data_sets[0].logical_node_name,
                model_plan.data_sets[0].name);
        }
        if (model_plan.report_count > 0U) {
            printf("firstModelReport=%s\n", model_plan.reports[0].key);
            printf("firstModelReportRptID=%s\n", model_plan.reports[0].rpt_id);
            printf("firstModelReportBuffered=%s\n", model_plan.reports[0].is_buffered ? "true" : "false");
            printf("firstModelReportConfRevKnown=%s\n", model_plan.reports[0].conf_rev_known ? "true" : "false");
            printf("firstModelReportConfRev=%" PRIu32 "\n", model_plan.reports[0].conf_rev);
            printf("firstModelReportBufTm=%" PRIu32 "\n", model_plan.reports[0].buffer_time_ms);
            printf("firstModelReportIntgPd=%" PRIu32 "\n", model_plan.reports[0].integrity_period_ms);
            printf("firstModelReportTrgOpsMask=%u\n", (unsigned int)model_plan.reports[0].trigger_options_mask);
            printf("firstModelReportOptFldsMask=%u\n", (unsigned int)model_plan.reports[0].optional_fields_mask);
        }
        if (model_plan.signal_count > 0U) {
            printf(
                "firstModelSignal=%s/%s.%s[%s]\n",
                model_plan.signals[0].logical_device_inst,
                model_plan.signals[0].logical_node_name,
                model_plan.signals[0].object_reference,
                model_plan.signals[0].fc);
            printf("firstModelSignalKind=%s\n", model_plan.signals[0].kind);
            printf("firstModelSignalDO=%s\n", model_plan.signals[0].data_object_name);
            printf("firstModelSignalDA=%s\n", model_plan.signals[0].data_attribute_path);
            printf("firstModelSignalDataSetEntryVariable=%s\n", model_plan.signals[0].data_set_entry_variable);
            printf(
                "firstModelSignalDataSetEntryComponent=%s\n",
                model_plan.signals[0].data_set_entry_component_known
                    ? model_plan.signals[0].data_set_entry_component
                    : "<none>");
            printf("firstModelSignalValueKind=%s\n", value_kind_label(model_plan.signals[0].initial_value_kind));
            printf("firstModelSignalInitialValue=%s\n", model_plan.signals[0].initial_value);
        }
        printf("bind=%s\n", options.bind_address);
        printf("port=%d\n", options.port);
        printf("libiec61850=%s\n", libiec61850_status());
        unitlab_free_ied_model_plan(&model_plan);
        unitlab_free_ied_fixture_model(&fixture_model);
        return 0;
    }

    UnitLabIedModelLoadResult load_result;
    UnitLabIedServerConfig server_config = {
        .bind_address = options.bind_address,
        .port = options.port,
    };
    if (options.metadata_probe) {
        if (!unitlab_probe_ied_server_metadata(&fixture_model, &model_plan, &server_config, &load_result)) {
            fprintf(stderr, "%s: %s\n", load_result.code, load_result.message);
            fprintf(stderr, "libiec61850=%s\n", libiec61850_status());
            unitlab_free_ied_model_plan(&model_plan);
            unitlab_free_ied_fixture_model(&fixture_model);
            return 69;
        }
        printf("unitlab-iec61850-ied-sim: metadata probe accepted\n");
        printf("ied=%s\n", fixture_model.ied_name);
        printf("endpoint=%s:%d\n", options.bind_address, options.port);
        printf("dataSets=%zu\n", model_plan.data_set_count);
        printf("reports=%zu\n", model_plan.report_count);
        printf("libiec61850=%s\n", libiec61850_status());
        unitlab_free_ied_model_plan(&model_plan);
        unitlab_free_ied_fixture_model(&fixture_model);
        return 0;
    }
    if (options.gi_probe) {
        if (!unitlab_probe_ied_server_gi(&fixture_model, &model_plan, &server_config, options.report_key, &load_result)) {
            fprintf(stderr, "%s: %s\n", load_result.code, load_result.message);
            fprintf(stderr, "libiec61850=%s\n", libiec61850_status());
            unitlab_free_ied_model_plan(&model_plan);
            unitlab_free_ied_fixture_model(&fixture_model);
            return 69;
        }
        printf("unitlab-iec61850-ied-sim: GI probe accepted\n");
        printf("ied=%s\n", fixture_model.ied_name);
        printf("endpoint=%s:%d\n", options.bind_address, options.port);
        if (options.report_key != NULL) {
            printf("reportKey=%s\n", options.report_key);
            printf("reports=1\n");
        }
        else {
            printf("reports=%zu\n", model_plan.report_count);
        }
        printf("libiec61850=%s\n", libiec61850_status());
        unitlab_free_ied_model_plan(&model_plan);
        unitlab_free_ied_fixture_model(&fixture_model);
        return 0;
    }
    if (options.native_smoke_start) {
        UnitLabMmsServerRuntime server_runtime;
        UnitLabMmsDiagnostic server_diagnostic;
        UnitLabMmsOperationResult operation_result;
        uint8_t association_bytes[256];
        size_t association_length = 0U;
        size_t consumed_length = 0U;

        unitlab_mms_server_runtime_init(&server_runtime);
        unitlab_mms_diagnostic_clear(&server_diagnostic);
        unitlab_mms_operation_result_init(&operation_result);
        if (!unitlab_mms_server_runtime_prepare(&server_runtime, &server_config, &server_diagnostic)) {
            fprintf(stderr, "%s: %s\n", "NATIVE_SERVER_PREPARE_FAILED", server_diagnostic.message);
            unitlab_free_ied_model_plan(&model_plan);
            unitlab_free_ied_fixture_model(&fixture_model);
            return 69;
        }
        if (!unitlab_mms_server_runtime_start(&server_runtime, &server_diagnostic)) {
            fprintf(stderr, "%s: %s\n", "NATIVE_SERVER_START_FAILED", server_diagnostic.message);
            unitlab_free_ied_model_plan(&model_plan);
            unitlab_free_ied_fixture_model(&fixture_model);
            return 69;
        }
        if (!unitlab_mms_server_runtime_reserve_report_control(&server_runtime, &server_diagnostic)
            || !unitlab_mms_server_runtime_enable_report_control(&server_runtime, &server_diagnostic)
            || !unitlab_mms_server_runtime_request_general_interrogation(&server_runtime, &server_diagnostic)) {
            fprintf(stderr, "%s: %s\n", "NATIVE_SERVER_REPORT_SETUP_FAILED", server_diagnostic.message);
            unitlab_free_ied_model_plan(&model_plan);
            unitlab_free_ied_fixture_model(&fixture_model);
            return 69;
        }
        if (!build_native_information_report_association_bytes(association_bytes, sizeof(association_bytes), &association_length, &server_diagnostic)) {
            fprintf(stderr, "%s: %s\n", "NATIVE_SERVER_ASSOCIATION_BUILD_FAILED", server_diagnostic.message);
            unitlab_free_ied_model_plan(&model_plan);
            unitlab_free_ied_fixture_model(&fixture_model);
            return 69;
        }
        if (!unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, association_bytes, association_length, &consumed_length, &operation_result)) {
            fprintf(stderr, "%s: %s\n", "NATIVE_SERVER_ASSOCIATION_APPLY_FAILED", operation_result.diagnostic.message);
            unitlab_free_ied_model_plan(&model_plan);
            unitlab_free_ied_fixture_model(&fixture_model);
            return 69;
        }
        if (consumed_length != association_length) {
            fprintf(stderr, "NATIVE_SERVER_ASSOCIATION_TAIL: consumed=%zu total=%zu\n", consumed_length, association_length);
            unitlab_free_ied_model_plan(&model_plan);
            unitlab_free_ied_fixture_model(&fixture_model);
            return 69;
        }
        printf("unitlab-iec61850-ied-sim: native server smoke-start accepted\n");
        printf("ied=%s\n", fixture_model.ied_name);
        printf("bind=%s\n", options.bind_address);
        printf("port=%d\n", options.port);
        printf("runtime=%d\n", server_runtime.state);
        printf("reportControl=%d\n", server_runtime.report_control.state);
        unitlab_free_ied_model_plan(&model_plan);
        unitlab_free_ied_fixture_model(&fixture_model);
        return 0;
    }

    if (options.smoke_start) {
        if (!unitlab_run_ied_server(&fixture_model, &model_plan, &server_config, immediate_stop_requested, NULL, &load_result)) {
            fprintf(stderr, "%s: %s\n", load_result.code, load_result.message);
            fprintf(stderr, "libiec61850=%s\n", libiec61850_status());
            unitlab_free_ied_model_plan(&model_plan);
            unitlab_free_ied_fixture_model(&fixture_model);
            return 69;
        }
        printf("unitlab-iec61850-ied-sim: server smoke-start accepted\n");
        printf("ied=%s\n", fixture_model.ied_name);
        printf("bind=%s\n", options.bind_address);
        printf("port=%d\n", options.port);
        printf("libiec61850=%s\n", libiec61850_status());
        unitlab_free_ied_model_plan(&model_plan);
        unitlab_free_ied_fixture_model(&fixture_model);
        return 0;
    }

    signal(SIGINT, handle_stop_signal);
    signal(SIGTERM, handle_stop_signal);
    if (!unitlab_run_ied_server(&fixture_model, &model_plan, &server_config, signal_stop_requested, NULL, &load_result)) {
        fprintf(stderr, "%s: %s\n", load_result.code, load_result.message);
        fprintf(stderr, "libiec61850=%s\n", libiec61850_status());
        unitlab_free_ied_model_plan(&model_plan);
        unitlab_free_ied_fixture_model(&fixture_model);
        return 69;
    }

    unitlab_free_ied_model_plan(&model_plan);
    unitlab_free_ied_fixture_model(&fixture_model);
    return 0;
}
