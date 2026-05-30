#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fixture_parser.h"
#include "model_loader.h"
#include "model_plan.h"

typedef struct SimulatorOptions {
    const char* fixture_path;
    const char* ied_name;
    const char* bind_address;
    int port;
    int dry_run;
    int smoke_start;
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
    printf("Usage: %s --fixture PATH --ied NAME [--bind ADDRESS] [--port PORT] [--dry-run] [--smoke-start]\n", program_name);
    printf("\n");
    printf("Options:\n");
    printf("  --fixture PATH   UnitLab IEC 61850 IED simulator fixture JSON.\n");
    printf("  --ied NAME       IED name from the fixture to expose.\n");
    printf("  --bind ADDRESS   Bind address for the MMS server. Default: 0.0.0.0.\n");
    printf("  --port PORT      TCP port for the MMS server. Default: 102.\n");
    printf("  --dry-run        Validate CLI and fixture boundary without opening MMS.\n");
    printf("  --smoke-start    Start and stop the linked MMS server once, then exit.\n");
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
    if (options->dry_run && options->smoke_start) {
        fprintf(stderr, "INVALID_ARGUMENT: --dry-run and --smoke-start are mutually exclusive.\n");
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
