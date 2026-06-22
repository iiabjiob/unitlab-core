#include "app/cli/unitlab_cli_options.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int unitlab_cli_parse_int(const char* value, int* out)
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

int unitlab_cli_parse_uint32(const char* value, uint32_t* out)
{
    char* end = NULL;
    unsigned long parsed = strtoul(value, &end, 10);
    if (value == end || end == NULL || *end != '\0' || parsed == 0UL || parsed > UINT32_MAX) {
        return 0;
    }
    *out = (uint32_t)parsed;
    return 1;
}

void unitlab_cli_print_usage(const char* program_name)
{
    printf("Usage: %s [--fixture PATH | --scl PATH] [--ied NAME] [--bind ADDRESS] [--port PORT] [--dry-run] [--smoke-start] [--native-smoke-start] [--native-wire-start] [--native-wire-client-start] [--mms-client-start] [--discover-probe] [--metadata-probe] [--gi-probe] [--report-key KEY] [--native-test-report-tick-ms MS]\n", program_name);
    printf("\n");
    printf("Options:\n");
    printf("  --fixture PATH   UnitLab IEC 61850 IED simulator fixture JSON.\n");
    printf("  --scl PATH       SCL/SCD source compiled by the native SCL compiler for native wire server mode.\n");
    printf("  --ied NAME       IED name from the fixture to expose. Optional for --mms-client-start.\n");
    printf("  --bind ADDRESS   Bind address for the MMS server. Default: 0.0.0.0.\n");
    printf("  --port PORT      TCP port for the MMS server. Default: 102.\n");
    printf("  --dry-run        Validate CLI and fixture boundary without opening MMS.\n");
    printf("  --smoke-start    Start and stop the linked MMS server once, then exit.\n");
    printf("  --native-smoke-start Exercise the native server-runtime boundary once, then exit.\n");
    printf("  --native-wire-start  Start a native wire server that can emit live reports over TCP.\n");
    printf("  --native-wire-client-start  Start a native wire client that connects and emits wire frames.\n");
    printf("  --mms-client-start  Start a persistent native wire MMS client session. Can run without --fixture/--scl.\n");
    printf("  --native-client-read-domain DOMAIN  Optional startup native wire client Read domain.\n");
    printf("  --native-client-read-item ITEM      Optional startup native wire client Read item.\n");
    printf("  --native-client-read-invoke-id ID   Optional startup native wire client Read invokeId. Default when enabled: 3.\n");
    printf("  --discover-probe Connect to the endpoint and print the LD/LN/DataSet/ReportControl browse tree, then exit.\n");
    printf("  --metadata-probe Connect to the endpoint and verify DataSet/BRCB metadata, then exit.\n");
    printf("  --gi-probe       Connect to the endpoint, enable report(s), request GI, verify fixture values, then exit.\n");
    printf("  --report-key KEY Limit --gi-probe validation to one fixture ReportControl key.\n");
    printf("  --native-test-report-tick-ms MS  With --native-wire-start, emit cyclic PGGIO1 data-change reports after RptEna. Default: 0/off.\n");
    printf("  --help           Show this help text.\n");
}

UnitLabCliCommandKind unitlab_cli_select_fixture_command(const UnitLabCliOptions* options)
{
    if (options->dry_run) {
        return UNITLAB_CLI_COMMAND_FIXTURE_DRY_RUN;
    }
    if (options->discover_probe) {
        return UNITLAB_CLI_COMMAND_FIXTURE_DISCOVER_PROBE;
    }
    if (options->metadata_probe) {
        return UNITLAB_CLI_COMMAND_FIXTURE_METADATA_PROBE;
    }
    if (options->gi_probe) {
        return UNITLAB_CLI_COMMAND_FIXTURE_GI_PROBE;
    }
    if (options->native_smoke_start) {
        return UNITLAB_CLI_COMMAND_FIXTURE_NATIVE_SMOKE_START;
    }
    if (options->native_wire_client_start) {
        return UNITLAB_CLI_COMMAND_FIXTURE_NATIVE_WIRE_CLIENT_START;
    }
    if (options->native_wire_start) {
        return UNITLAB_CLI_COMMAND_FIXTURE_NATIVE_WIRE_START;
    }
    if (options->smoke_start) {
        return UNITLAB_CLI_COMMAND_FIXTURE_SMOKE_START;
    }
    return UNITLAB_CLI_COMMAND_FIXTURE_SERVER;
}

UnitLabCliCommandKind unitlab_cli_select_scl_command(const UnitLabCliOptions* options)
{
    if (options->dry_run) {
        return UNITLAB_CLI_COMMAND_SCL_DRY_RUN;
    }
    if (options->discover_probe) {
        return UNITLAB_CLI_COMMAND_SCL_DISCOVER_PROBE;
    }
    if (options->metadata_probe) {
        return UNITLAB_CLI_COMMAND_SCL_METADATA_PROBE;
    }
    if (options->gi_probe) {
        return UNITLAB_CLI_COMMAND_SCL_GI_PROBE;
    }
    if (options->native_smoke_start) {
        return UNITLAB_CLI_COMMAND_SCL_NATIVE_SMOKE_START;
    }
    if (options->native_wire_start) {
        return UNITLAB_CLI_COMMAND_SCL_NATIVE_WIRE_START;
    }
    return UNITLAB_CLI_COMMAND_SCL_SERVER;
}

int unitlab_cli_parse_options(int argc, char** argv, UnitLabCliContext* context)
{
    UnitLabCliOptions* options = &context->options;

    memset(context, 0, sizeof(*context));
    options->fixture_path = NULL;
    options->scl_path = NULL;
    options->ied_name = NULL;
    options->bind_address = "0.0.0.0";
    options->port = 102;
    options->dry_run = 0;
    options->smoke_start = 0;
    options->native_smoke_start = 0;
    options->native_wire_start = 0;
    options->native_wire_client_start = 0;
    options->mms_client_start = 0;
    options->discover_probe = 0;
    options->metadata_probe = 0;
    options->gi_probe = 0;
    options->native_test_report_tick_ms = 0;
    options->report_key = NULL;
    options->native_client_read_domain = NULL;
    options->native_client_read_item = NULL;
    options->native_client_read_invoke_id = 0U;

    for (int index = 1; index < argc; index++) {
        const char* arg = argv[index];
        if (strcmp(arg, "--help") == 0) {
            unitlab_cli_print_usage(argv[0]);
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
        if (strcmp(arg, "--native-wire-start") == 0) {
            options->native_wire_start = 1;
            continue;
        }
        if (strcmp(arg, "--native-wire-client-start") == 0) {
            options->native_wire_client_start = 1;
            continue;
        }
        if (strcmp(arg, "--mms-client-start") == 0) {
            options->mms_client_start = 1;
            continue;
        }
        if (strcmp(arg, "--discover-probe") == 0) {
            options->discover_probe = 1;
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
        if (strcmp(arg, "--native-client-read-domain") == 0 && index + 1 < argc) {
            options->native_client_read_domain = argv[++index];
            continue;
        }
        if (strcmp(arg, "--native-client-read-item") == 0 && index + 1 < argc) {
            options->native_client_read_item = argv[++index];
            continue;
        }
        if (strcmp(arg, "--native-client-read-invoke-id") == 0 && index + 1 < argc) {
            if (!unitlab_cli_parse_uint32(argv[++index], &options->native_client_read_invoke_id)) {
                fprintf(stderr, "INVALID_NATIVE_CLIENT_READ_INVOKE_ID: expected invokeId in range 1..4294967295.\n");
                return -1;
            }
            continue;
        }
        if (strcmp(arg, "--native-test-report-tick-ms") == 0 && index + 1 < argc) {
            if (!unitlab_cli_parse_int(argv[++index], &options->native_test_report_tick_ms)) {
                fprintf(stderr, "INVALID_NATIVE_TEST_REPORT_TICK_MS: expected interval in range 1..65535.\n");
                return -1;
            }
            continue;
        }
        if (strcmp(arg, "--fixture") == 0 && index + 1 < argc) {
            options->fixture_path = argv[++index];
            continue;
        }
        if (strcmp(arg, "--scl") == 0 && index + 1 < argc) {
            options->scl_path = argv[++index];
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
            if (!unitlab_cli_parse_int(argv[++index], &options->port)) {
                fprintf(stderr, "INVALID_PORT: expected TCP port in range 1..65535.\n");
                return -1;
            }
            continue;
        }
        fprintf(stderr, "INVALID_ARGUMENT: unsupported or incomplete argument \"%s\".\n", arg);
        return -1;
    }

    if ((options->fixture_path == NULL || options->fixture_path[0] == '\0') && (options->scl_path == NULL || options->scl_path[0] == '\0') && !options->mms_client_start) {
        fprintf(stderr, "SOURCE_REQUIRED: --fixture PATH or --scl PATH is required unless --mms-client-start is used.\n");
        return -1;
    }
    if (options->fixture_path != NULL && options->fixture_path[0] != '\0' && options->scl_path != NULL && options->scl_path[0] != '\0') {
        fprintf(stderr, "INVALID_ARGUMENT: --fixture and --scl are mutually exclusive.\n");
        return -1;
    }
    if ((options->ied_name == NULL || options->ied_name[0] == '\0') && !options->mms_client_start) {
        fprintf(stderr, "IED_REQUIRED: --ied NAME is required.\n");
        return -1;
    }
    if (options->bind_address == NULL || options->bind_address[0] == '\0') {
        fprintf(stderr, "BIND_REQUIRED: --bind ADDRESS cannot be empty.\n");
        return -1;
    }
    if ((options->dry_run ? 1 : 0) + (options->smoke_start ? 1 : 0) + (options->native_smoke_start ? 1 : 0) + (options->native_wire_start ? 1 : 0) + (options->native_wire_client_start ? 1 : 0) + (options->mms_client_start ? 1 : 0) + (options->discover_probe ? 1 : 0) + (options->metadata_probe ? 1 : 0) + (options->gi_probe ? 1 : 0) > 1) {
        fprintf(stderr, "INVALID_ARGUMENT: --dry-run, --smoke-start, --native-smoke-start, --native-wire-start, --native-wire-client-start, --mms-client-start, --discover-probe, --metadata-probe, and --gi-probe are mutually exclusive.\n");
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
    if (options->native_test_report_tick_ms != 0 && !options->native_wire_start) {
        fprintf(stderr, "INVALID_ARGUMENT: --native-test-report-tick-ms requires --native-wire-start.\n");
        return -1;
    }
    if (options->native_client_read_domain != NULL && options->native_client_read_domain[0] == '\0') {
        fprintf(stderr, "INVALID_ARGUMENT: --native-client-read-domain cannot be empty.\n");
        return -1;
    }
    if (options->native_client_read_item != NULL && options->native_client_read_item[0] == '\0') {
        fprintf(stderr, "INVALID_ARGUMENT: --native-client-read-item cannot be empty.\n");
        return -1;
    }
    if ((options->native_client_read_domain != NULL || options->native_client_read_item != NULL || options->native_client_read_invoke_id != 0U) && !(options->native_wire_client_start || options->mms_client_start)) {
        fprintf(stderr, "INVALID_ARGUMENT: native client Read options require --native-wire-client-start or --mms-client-start.\n");
        return -1;
    }
    if ((options->native_client_read_domain != NULL) != (options->native_client_read_item != NULL)) {
        fprintf(stderr, "INVALID_ARGUMENT: startup native client Read requires both --native-client-read-domain and --native-client-read-item.\n");
        return -1;
    }
    if (options->native_client_read_invoke_id != 0U && (options->native_client_read_domain == NULL || options->native_client_read_item == NULL)) {
        fprintf(stderr, "INVALID_ARGUMENT: --native-client-read-invoke-id requires startup native client Read domain and item.\n");
        return -1;
    }

    context->server_config.bind_address = options->bind_address;
    context->server_config.port = options->port;
    context->server_config.control_port = options->port < 65535 ? options->port + 1 : 0;
    context->server_config.native_test_report_tick_ms = options->native_test_report_tick_ms;
    context->wire_client_options.initial_read_domain = options->native_client_read_domain;
    context->wire_client_options.initial_read_item = options->native_client_read_item;
    context->wire_client_options.initial_read_invoke_id = options->native_client_read_invoke_id;
    context->wire_client_options.initial_read_enabled = options->native_client_read_domain != NULL && options->native_client_read_item != NULL;

    if (options->mms_client_start) {
        context->source_kind = UNITLAB_CLI_SOURCE_MMS_CLIENT;
        context->command_kind = UNITLAB_CLI_COMMAND_MMS_CLIENT_START;
    }
    else if (options->scl_path != NULL && options->scl_path[0] != '\0') {
        context->source_kind = UNITLAB_CLI_SOURCE_SCL;
        context->command_kind = unitlab_cli_select_scl_command(options);
    }
    else {
        context->source_kind = UNITLAB_CLI_SOURCE_FIXTURE;
        context->command_kind = unitlab_cli_select_fixture_command(options);
    }

    return 0;
}
