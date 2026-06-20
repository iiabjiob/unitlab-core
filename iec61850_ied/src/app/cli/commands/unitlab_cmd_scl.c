#include "app/cli/commands/unitlab_cmd_scl.h"

/*
 * SCL-backed CLI command family: dry-run, simulator start, smoke probes, and
 * SCL-loaded discovery/report command paths.
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/client_probe.h"
#include "app/cli/unitlab_cli_internal.h"
#include "scl_compiler/unitlab_scl_compiler.h"
#include "server/native_wire_server.h"
#include "server/unitlab_mms_server_runtime.h"

static int run_scl_dry_run(const UnitLabCliContext* context, const UnitLabIedModelPlan* model_plan)
{
    printf("unitlab-iec61850-ied-sim: SCL accepted\n");
    printf("ied=%s\n", context->options.ied_name);
    printf("schema=unitlab.iec61850.scl.normalized.v1\n");
    printf("modelLogicalDevices=%zu\n", model_plan->logical_device_count);
    printf("modelLogicalNodes=%zu\n", model_plan->logical_node_count);
    printf("modelDataSets=%zu\n", model_plan->data_set_count);
    printf("modelReports=%zu\n", model_plan->report_count);
    printf("modelSignals=%zu\n", model_plan->signal_count);
    printf("bind=%s\n", context->options.bind_address);
    printf("port=%d\n", context->options.port);
    printf("mms-backend=%s\n", unitlab_cli_mms_backend_status());
    return 0;
}

int unitlab_cli_run_scl_command(const UnitLabCliContext* context)
{
    const UnitLabCliOptions* options;
    char* scl_text;
    UnitLabSclCompileResult* compile_result = NULL;
    const UnitLabIedModelPlan* model_plan = NULL;
    char compile_error[512];
    UnitLabIedServerConfig server_config;

    if (context == NULL) {
        fprintf(stderr, "INVALID_ARGUMENT: missing CLI context.\n");
        return 64;
    }

    options = &context->options;
    if (options->native_wire_client_start) {
        fprintf(stderr, "INVALID_ARGUMENT: --scl currently supports --dry-run, --smoke-start, --native-smoke-start, --native-wire-start, --mms-client-start, --discover-probe, --metadata-probe, --gi-probe, or linked server start.\n");
        return 64;
    }

    scl_text = unitlab_cli_read_text_file(options->scl_path);
    if (scl_text == NULL) {
        return 66;
    }

    compile_error[0] = '\0';
    if (!unitlab_scl_compile_from_memory(scl_text, strlen(scl_text), options->ied_name, &compile_result, compile_error, sizeof(compile_error))) {
        fprintf(stderr, "SCL_COMPILE_FAILED: %s\n", compile_error[0] != '\0' ? compile_error : "native SCL compiler failed");
        free(scl_text);
        return 65;
    }
    free(scl_text);

    model_plan = unitlab_scl_compile_model_plan(compile_result);
    if (model_plan == NULL) {
        fprintf(stderr, "SCL_MODEL_PLAN_MISSING: native SCL compiler did not return a model plan.\n");
        unitlab_scl_compile_result_free(compile_result);
        return 65;
    }
    if (model_plan->logical_device_count == 0U) {
        fprintf(stderr, "SCL_MODEL_EMPTY: selected IED %s has no MMS LogicalDevice in the compiled model.\n", options->ied_name);
        unitlab_scl_compile_result_free(compile_result);
        return 65;
    }

    if (options->dry_run) {
        return run_scl_dry_run(context, model_plan);
    }

    server_config = context->server_config;

    if (options->discover_probe) {
        UnitLabIedModelLoadResult probe_result;

        if (!unitlab_probe_ied_server_discovery(&server_config, &probe_result)) {
            fprintf(stderr, "%s: %s\n", probe_result.code, probe_result.message);
            fprintf(stderr, "mms-backend=%s\n", unitlab_cli_mms_backend_status());
            unitlab_scl_compile_result_free(compile_result);
            return 69;
        }
        printf("unitlab-iec61850-ied-sim: SCL discover probe accepted\n");
        printf("ied=%s\n", options->ied_name);
        printf("endpoint=%s:%d\n", options->bind_address, options->port);
        printf("mms-backend=%s\n", unitlab_cli_mms_backend_status());
        unitlab_scl_compile_result_free(compile_result);
        return 0;
    }
    if (options->metadata_probe) {
        UnitLabIedFixtureModel fixture_model;
        UnitLabIedModelLoadResult probe_result;

        unitlab_cli_initialize_synthetic_fixture(&fixture_model, options->ied_name);
        if (!unitlab_probe_ied_server_metadata(&fixture_model, model_plan, &server_config, &probe_result)) {
            fprintf(stderr, "%s: %s\n", probe_result.code, probe_result.message);
            fprintf(stderr, "mms-backend=%s\n", unitlab_cli_mms_backend_status());
            unitlab_scl_compile_result_free(compile_result);
            return 69;
        }
        printf("unitlab-iec61850-ied-sim: SCL metadata probe accepted\n");
        printf("ied=%s\n", options->ied_name);
        printf("endpoint=%s:%d\n", options->bind_address, options->port);
        printf("dataSets=%zu\n", model_plan->data_set_count);
        printf("reports=%zu\n", model_plan->report_count);
        printf("mms-backend=%s\n", unitlab_cli_mms_backend_status());
        unitlab_scl_compile_result_free(compile_result);
        return 0;
    }
    if (options->gi_probe) {
        UnitLabIedFixtureModel fixture_model;
        UnitLabIedModelLoadResult probe_result;

        unitlab_cli_initialize_synthetic_fixture(&fixture_model, options->ied_name);
        if (!unitlab_probe_ied_server_gi(&fixture_model, model_plan, &server_config, options->report_key, &probe_result)) {
            fprintf(stderr, "%s: %s\n", probe_result.code, probe_result.message);
            fprintf(stderr, "mms-backend=%s\n", unitlab_cli_mms_backend_status());
            unitlab_scl_compile_result_free(compile_result);
            return 69;
        }
        printf("unitlab-iec61850-ied-sim: SCL GI probe accepted\n");
        printf("ied=%s\n", options->ied_name);
        printf("endpoint=%s:%d\n", options->bind_address, options->port);
        if (options->report_key != NULL) {
            printf("reportKey=%s\n", options->report_key);
            printf("reports=1\n");
        }
        else {
            printf("reports=%zu\n", model_plan->report_count);
        }
        printf("mms-backend=%s\n", unitlab_cli_mms_backend_status());
        unitlab_scl_compile_result_free(compile_result);
        return 0;
    }
    if (options->native_smoke_start) {
        UnitLabMmsServerRuntime server_runtime;
        UnitLabMmsDiagnostic server_diagnostic;

        unitlab_mms_server_runtime_init(&server_runtime);
        unitlab_mms_server_runtime_apply_model_plan(&server_runtime, model_plan);
        unitlab_mms_diagnostic_clear(&server_diagnostic);
        if (!unitlab_mms_server_runtime_prepare(&server_runtime, &server_config, &server_diagnostic)
            || !unitlab_mms_server_runtime_start(&server_runtime, &server_diagnostic)) {
            fprintf(stderr, "SCL_NATIVE_SERVER_START_FAILED: %s\n", server_diagnostic.message);
            unitlab_scl_compile_result_free(compile_result);
            return 69;
        }
        printf("unitlab-iec61850-ied-sim: SCL native server smoke-start accepted\n");
        printf("ied=%s\n", options->ied_name);
        printf("bind=%s\n", options->bind_address);
        printf("port=%d\n", options->port);
        printf("runtime=%d\n", server_runtime.state);
        unitlab_scl_compile_result_free(compile_result);
        return 0;
    }
    if (options->native_wire_start) {
        UnitLabMmsServerRuntime server_runtime;
        UnitLabMmsDiagnostic server_diagnostic;
        UnitLabIedModelLoadResult wire_result;

        unitlab_mms_server_runtime_init(&server_runtime);
        unitlab_mms_server_runtime_apply_model_plan(&server_runtime, model_plan);
        unitlab_mms_diagnostic_clear(&server_diagnostic);
        if (!unitlab_mms_server_runtime_prepare(&server_runtime, &server_config, &server_diagnostic)) {
            fprintf(stderr, "%s: %s\n", "NATIVE_WIRE_SERVER_PREPARE_FAILED", server_diagnostic.message);
            unitlab_scl_compile_result_free(compile_result);
            return 69;
        }
        if (!unitlab_mms_server_runtime_start(&server_runtime, &server_diagnostic)) {
            fprintf(stderr, "%s: %s\n", "NATIVE_WIRE_SERVER_START_FAILED", server_diagnostic.message);
            unitlab_scl_compile_result_free(compile_result);
            return 69;
        }
        if (!unitlab_run_native_wire_server(&server_runtime, &server_config, &wire_result, unitlab_cli_signal_stop_requested, NULL)) {
            fprintf(stderr, "%s: %s\n", wire_result.code, wire_result.message);
            unitlab_scl_compile_result_free(compile_result);
            return 69;
        }
        printf("unitlab-iec61850-ied-sim: SCL native wire server stopped\n");
        printf("ied=%s\n", options->ied_name);
        printf("bind=%s\n", options->bind_address);
        printf("port=%d\n", options->port);
        unitlab_scl_compile_result_free(compile_result);
        return 0;
    }
    if (options->smoke_start) {
        UnitLabIedFixtureModel fixture_model;
        UnitLabIedModelLoadResult load_result;

        unitlab_cli_initialize_synthetic_fixture(&fixture_model, options->ied_name);
        if (!unitlab_run_ied_server(&fixture_model, model_plan, &server_config, unitlab_cli_immediate_stop_requested, NULL, &load_result)) {
            fprintf(stderr, "%s: %s\n", load_result.code, load_result.message);
            fprintf(stderr, "mms-backend=%s\n", unitlab_cli_mms_backend_status());
            unitlab_scl_compile_result_free(compile_result);
            return 69;
        }
        printf("unitlab-iec61850-ied-sim: SCL-backed MMS server smoke-start accepted\n");
        printf("ied=%s\n", options->ied_name);
        printf("bind=%s\n", options->bind_address);
        printf("port=%d\n", options->port);
        printf("mms-backend=%s\n", unitlab_cli_mms_backend_status());
        unitlab_scl_compile_result_free(compile_result);
        return 0;
    }

    {
        UnitLabIedFixtureModel fixture_model;
        UnitLabIedModelLoadResult load_result;

        unitlab_cli_initialize_synthetic_fixture(&fixture_model, options->ied_name);
        unitlab_cli_install_stop_handlers();
        if (!unitlab_run_ied_server(&fixture_model, model_plan, &server_config, unitlab_cli_signal_stop_requested, NULL, &load_result)) {
            fprintf(stderr, "%s: %s\n", load_result.code, load_result.message);
            fprintf(stderr, "mms-backend=%s\n", unitlab_cli_mms_backend_status());
            unitlab_scl_compile_result_free(compile_result);
            return 69;
        }
        unitlab_scl_compile_result_free(compile_result);
        return 0;
    }
}
