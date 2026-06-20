#include "app/cli/commands/unitlab_cmd_fixture.h"

/*
 * Fixture-backed CLI command family: dry-run, simulator start, smoke probes,
 * native wire server/client, and discovery/metadata/GI probes.
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/client_probe.h"
#include "app/cli/unitlab_cli_internal.h"
#include "fixture/fixture_parser.h"
#include "model/model_loader.h"
#include "model/model_plan.h"
#include "server/native_wire_server.h"
#include "server/unitlab_mms_server_runtime.h"

static void print_fixture_dry_run(
    const UnitLabIedFixtureModel* fixture_model,
    const UnitLabIedModelPlan* model_plan,
    const UnitLabCliContext* context)
{
    printf("unitlab-iec61850-ied-sim: fixture accepted\n");
    printf("schema=%s\n", UNITLAB_IED_SIM_SCHEMA);
    printf("ied=%s\n", fixture_model->ied_name);
    printf("accessPoint=%s\n", fixture_model->access_point_name);
    printf("devices=%zu\n", fixture_model->device_count);
    printf("dataSets=%zu\n", fixture_model->data_set_count);
    printf("reports=%zu\n", fixture_model->report_count);
    printf("signals=%zu\n", fixture_model->signal_count);
    if (fixture_model->data_set_count > 0U) {
        printf("firstDataSet=%s\n", fixture_model->data_sets[0].reference);
    }
    if (fixture_model->data_set_count > 0U && fixture_model->data_sets[0].signal_count > 0U) {
        printf("firstSignal=%s\n", fixture_model->data_sets[0].signals[0].reference);
    }
    if (fixture_model->report_count > 0U) {
        printf("firstReport=%s\n", fixture_model->reports[0].key);
        printf("firstReportTriggerGI=%s\n", unitlab_cli_optional_bool_label(fixture_model->reports[0].trigger_options.general_interrogation));
        printf("firstReportOptDataRef=%s\n", unitlab_cli_optional_bool_label(fixture_model->reports[0].optional_fields.data_reference));
    }
    printf("modelLogicalDevices=%zu\n", model_plan->logical_device_count);
    printf("modelLogicalNodes=%zu\n", model_plan->logical_node_count);
    printf("modelDataSets=%zu\n", model_plan->data_set_count);
    printf("modelReports=%zu\n", model_plan->report_count);
    printf("modelSignals=%zu\n", model_plan->signal_count);
    if (model_plan->logical_device_count > 0U) {
        printf("firstModelLogicalDevice=%s\n", model_plan->logical_devices[0].inst);
    }
    if (model_plan->logical_node_count > 0U) {
        printf("firstModelLogicalNode=%s/%s\n", model_plan->logical_nodes[0].logical_device_inst, model_plan->logical_nodes[0].name);
    }
    if (model_plan->data_set_count > 0U) {
        printf(
            "firstModelDataSet=%s/%s.%s\n",
            model_plan->data_sets[0].logical_device_inst,
            model_plan->data_sets[0].logical_node_name,
            model_plan->data_sets[0].name);
    }
    if (model_plan->report_count > 0U) {
        printf("firstModelReport=%s\n", model_plan->reports[0].key);
        printf("firstModelReportRptID=%s\n", model_plan->reports[0].rpt_id);
        printf("firstModelReportBuffered=%s\n", model_plan->reports[0].is_buffered ? "true" : "false");
        printf("firstModelReportConfRevKnown=%s\n", model_plan->reports[0].conf_rev_known ? "true" : "false");
        printf("firstModelReportConfRev=%" PRIu32 "\n", model_plan->reports[0].conf_rev);
        printf("firstModelReportBufTm=%" PRIu32 "\n", model_plan->reports[0].buffer_time_ms);
        printf("firstModelReportIntgPd=%" PRIu32 "\n", model_plan->reports[0].integrity_period_ms);
        printf("firstModelReportTrgOpsMask=%u\n", (unsigned int)model_plan->reports[0].trigger_options_mask);
        printf("firstModelReportOptFldsMask=%u\n", (unsigned int)model_plan->reports[0].optional_fields_mask);
    }
    if (model_plan->signal_count > 0U) {
        printf(
            "firstModelSignal=%s/%s.%s[%s]\n",
            model_plan->signals[0].logical_device_inst,
            model_plan->signals[0].logical_node_name,
            model_plan->signals[0].object_reference,
            model_plan->signals[0].fc);
        printf("firstModelSignalKind=%s\n", model_plan->signals[0].kind);
        printf("firstModelSignalDO=%s\n", model_plan->signals[0].data_object_name);
        printf("firstModelSignalDA=%s\n", model_plan->signals[0].data_attribute_path);
        printf("firstModelSignalDataSetEntryVariable=%s\n", model_plan->signals[0].data_set_entry_variable);
        printf(
            "firstModelSignalDataSetEntryComponent=%s\n",
            model_plan->signals[0].data_set_entry_component_known ? model_plan->signals[0].data_set_entry_component : "<none>");
        printf("firstModelSignalValueKind=%s\n", unitlab_cli_value_kind_label(model_plan->signals[0].initial_value_kind));
        printf("firstModelSignalInitialValue=%s\n", model_plan->signals[0].initial_value);
    }
    printf("bind=%s\n", context->options.bind_address);
    printf("port=%d\n", context->options.port);
    printf("mms-backend=%s\n", unitlab_cli_mms_backend_status());
}

static int run_fixture_discovery_probe(const UnitLabCliContext* context, const UnitLabIedFixtureModel* fixture_model, const UnitLabIedModelPlan* model_plan)
{
    UnitLabIedModelLoadResult load_result;
    (void)model_plan;

    if (!unitlab_probe_ied_server_discovery(&context->server_config, &load_result)) {
        fprintf(stderr, "%s: %s\n", load_result.code, load_result.message);
        fprintf(stderr, "mms-backend=%s\n", unitlab_cli_mms_backend_status());
        return 69;
    }
    printf("unitlab-iec61850-ied-sim: discover probe accepted\n");
    printf("ied=%s\n", fixture_model->ied_name);
    printf("endpoint=%s:%d\n", context->options.bind_address, context->options.port);
    printf("mms-backend=%s\n", unitlab_cli_mms_backend_status());
    return 0;
}

static int run_fixture_metadata_probe(const UnitLabCliContext* context, const UnitLabIedFixtureModel* fixture_model, const UnitLabIedModelPlan* model_plan)
{
    UnitLabIedModelLoadResult load_result;

    if (!unitlab_probe_ied_server_metadata(fixture_model, model_plan, &context->server_config, &load_result)) {
        fprintf(stderr, "%s: %s\n", load_result.code, load_result.message);
        fprintf(stderr, "mms-backend=%s\n", unitlab_cli_mms_backend_status());
        return 69;
    }
    printf("unitlab-iec61850-ied-sim: metadata probe accepted\n");
    printf("ied=%s\n", fixture_model->ied_name);
    printf("endpoint=%s:%d\n", context->options.bind_address, context->options.port);
    printf("dataSets=%zu\n", model_plan->data_set_count);
    printf("reports=%zu\n", model_plan->report_count);
    printf("mms-backend=%s\n", unitlab_cli_mms_backend_status());
    return 0;
}

static int run_fixture_gi_probe(const UnitLabCliContext* context, const UnitLabIedFixtureModel* fixture_model, const UnitLabIedModelPlan* model_plan)
{
    UnitLabIedModelLoadResult load_result;

    if (!unitlab_probe_ied_server_gi(fixture_model, model_plan, &context->server_config, context->options.report_key, &load_result)) {
        fprintf(stderr, "%s: %s\n", load_result.code, load_result.message);
        fprintf(stderr, "mms-backend=%s\n", unitlab_cli_mms_backend_status());
        return 69;
    }
    printf("unitlab-iec61850-ied-sim: GI probe accepted\n");
    printf("ied=%s\n", fixture_model->ied_name);
    printf("endpoint=%s:%d\n", context->options.bind_address, context->options.port);
    if (context->options.report_key != NULL) {
        printf("reportKey=%s\n", context->options.report_key);
        printf("reports=1\n");
    }
    else {
        printf("reports=%zu\n", model_plan->report_count);
    }
    printf("mms-backend=%s\n", unitlab_cli_mms_backend_status());
    return 0;
}

static int run_fixture_native_smoke_start(const UnitLabCliContext* context, const UnitLabIedFixtureModel* fixture_model, const UnitLabIedModelPlan* model_plan)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic server_diagnostic;
    UnitLabMmsOperationResult operation_result;
    uint8_t association_bytes[256];
    size_t association_length = 0U;
    size_t consumed_length = 0U;

    unitlab_mms_server_runtime_init(&server_runtime);
    unitlab_mms_server_runtime_apply_model_plan(&server_runtime, model_plan);
    unitlab_mms_diagnostic_clear(&server_diagnostic);
    unitlab_mms_operation_result_init(&operation_result);
    if (!unitlab_mms_server_runtime_prepare(&server_runtime, &context->server_config, &server_diagnostic)) {
        fprintf(stderr, "%s: %s\n", "NATIVE_SERVER_PREPARE_FAILED", server_diagnostic.message);
        return 69;
    }
    if (!unitlab_mms_server_runtime_start(&server_runtime, &server_diagnostic)) {
        fprintf(stderr, "%s: %s\n", "NATIVE_SERVER_START_FAILED", server_diagnostic.message);
        return 69;
    }
    if (!unitlab_mms_server_runtime_reserve_report_control(&server_runtime, &server_diagnostic)
        || !unitlab_mms_server_runtime_enable_report_control(&server_runtime, &server_diagnostic)
        || !unitlab_mms_server_runtime_request_general_interrogation(&server_runtime, &server_diagnostic)) {
        fprintf(stderr, "%s: %s\n", "NATIVE_SERVER_REPORT_SETUP_FAILED", server_diagnostic.message);
        return 69;
    }
    if (!unitlab_cli_build_native_association_request_bytes(association_bytes, sizeof(association_bytes), &association_length, &server_diagnostic)) {
        fprintf(stderr, "%s: %s\n", "NATIVE_SERVER_ASSOCIATION_BUILD_FAILED", server_diagnostic.message);
        return 69;
    }
    if (!unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, association_bytes, association_length, &consumed_length, &operation_result)) {
        fprintf(stderr, "%s: %s\n", "NATIVE_SERVER_ASSOCIATION_APPLY_FAILED", operation_result.diagnostic.message);
        return 69;
    }
    if (consumed_length != association_length) {
        fprintf(stderr, "NATIVE_SERVER_ASSOCIATION_TAIL: consumed=%zu total=%zu\n", consumed_length, association_length);
        return 69;
    }
    printf("unitlab-iec61850-ied-sim: native server smoke-start accepted\n");
    printf("ied=%s\n", fixture_model->ied_name);
    printf("bind=%s\n", context->options.bind_address);
    printf("port=%d\n", context->options.port);
    printf("runtime=%d\n", server_runtime.state);
    printf("reportControl=%d\n", server_runtime.report_control.state);
    return 0;
}

static int run_fixture_native_wire_client_start(const UnitLabCliContext* context, const UnitLabIedFixtureModel* fixture_model)
{
    UnitLabIedModelLoadResult wire_result;

    unitlab_cli_install_stop_handlers();
    if (!unitlab_run_native_wire_client_with_options(&context->server_config, &context->wire_client_options, &wire_result, unitlab_cli_signal_stop_requested, NULL)) {
        fprintf(stderr, "%s: %s\n", wire_result.code, wire_result.message);
        return 69;
    }
    printf("unitlab-iec61850-ied-sim: native wire client stopped\n");
    printf("ied=%s\n", fixture_model->ied_name);
    printf("bind=%s\n", context->options.bind_address);
    printf("port=%d\n", context->options.port);
    return 0;
}

static int run_fixture_native_wire_start(const UnitLabCliContext* context, const UnitLabIedFixtureModel* fixture_model, const UnitLabIedModelPlan* model_plan)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic server_diagnostic;
    UnitLabIedModelLoadResult wire_result;

    unitlab_mms_server_runtime_init(&server_runtime);
    unitlab_mms_server_runtime_apply_model_plan(&server_runtime, model_plan);
    unitlab_mms_diagnostic_clear(&server_diagnostic);
    if (!unitlab_mms_server_runtime_prepare(&server_runtime, &context->server_config, &server_diagnostic)) {
        fprintf(stderr, "%s: %s\n", "NATIVE_WIRE_SERVER_PREPARE_FAILED", server_diagnostic.message);
        return 69;
    }
    if (!unitlab_mms_server_runtime_start(&server_runtime, &server_diagnostic)) {
        fprintf(stderr, "%s: %s\n", "NATIVE_WIRE_SERVER_START_FAILED", server_diagnostic.message);
        return 69;
    }
    if (!unitlab_run_native_wire_server(&server_runtime, &context->server_config, &wire_result, unitlab_cli_signal_stop_requested, NULL)) {
        fprintf(stderr, "%s: %s\n", wire_result.code, wire_result.message);
        return 69;
    }
    printf("unitlab-iec61850-ied-sim: native wire server stopped\n");
    printf("ied=%s\n", fixture_model->ied_name);
    printf("bind=%s\n", context->options.bind_address);
    printf("port=%d\n", context->options.port);
    return 0;
}

static int run_fixture_server_smoke_start(const UnitLabCliContext* context, const UnitLabIedFixtureModel* fixture_model, const UnitLabIedModelPlan* model_plan)
{
    UnitLabIedModelLoadResult load_result;
    UnitLabIedServerStopRequested stop_requested = unitlab_cli_immediate_stop_requested;

    if (!unitlab_run_ied_server(fixture_model, model_plan, &context->server_config, stop_requested, NULL, &load_result)) {
        fprintf(stderr, "%s: %s\n", load_result.code, load_result.message);
        fprintf(stderr, "mms-backend=%s\n", unitlab_cli_mms_backend_status());
        return 69;
    }
    printf("unitlab-iec61850-ied-sim: server smoke-start accepted\n");
    printf("ied=%s\n", fixture_model->ied_name);
    printf("bind=%s\n", context->options.bind_address);
    printf("port=%d\n", context->options.port);
    printf("mms-backend=%s\n", unitlab_cli_mms_backend_status());
    return 0;
}

static int run_fixture_server_start(const UnitLabCliContext* context, const UnitLabIedFixtureModel* fixture_model, const UnitLabIedModelPlan* model_plan)
{
    UnitLabIedModelLoadResult load_result;

    unitlab_cli_install_stop_handlers();
    if (!unitlab_run_ied_server(fixture_model, model_plan, &context->server_config, unitlab_cli_signal_stop_requested, NULL, &load_result)) {
        fprintf(stderr, "%s: %s\n", load_result.code, load_result.message);
        fprintf(stderr, "mms-backend=%s\n", unitlab_cli_mms_backend_status());
        return 69;
    }
    return 0;
}

int unitlab_cli_run_fixture_command(const UnitLabCliContext* context)
{
    const UnitLabCliOptions* options;
    char* fixture_text;
    UnitLabIedFixtureModel fixture_model;
    char fixture_error[256];
    UnitLabIedModelPlan model_plan;
    char model_error[256];
    int valid;

    if (context == NULL) {
        fprintf(stderr, "INVALID_ARGUMENT: missing CLI context.\n");
        return 64;
    }

    options = &context->options;
    fixture_text = unitlab_cli_read_text_file(options->fixture_path);
    if (fixture_text == NULL) {
        return 66;
    }

    valid = unitlab_parse_ied_fixture_model(
        fixture_text,
        options->ied_name,
        &fixture_model,
        fixture_error,
        sizeof(fixture_error));
    free(fixture_text);
    if (!valid) {
        fprintf(stderr, "%s\n", fixture_error);
        return 65;
    }

    if (!unitlab_build_ied_model_plan(&fixture_model, &model_plan, model_error, sizeof(model_error))) {
        fprintf(stderr, "%s\n", model_error);
        unitlab_free_ied_fixture_model(&fixture_model);
        return 65;
    }

    if (options->dry_run) {
        print_fixture_dry_run(&fixture_model, &model_plan, context);
        unitlab_free_ied_model_plan(&model_plan);
        unitlab_free_ied_fixture_model(&fixture_model);
        return 0;
    }

    if (options->discover_probe) {
        int result = run_fixture_discovery_probe(context, &fixture_model, &model_plan);
        unitlab_free_ied_model_plan(&model_plan);
        unitlab_free_ied_fixture_model(&fixture_model);
        return result;
    }
    if (options->metadata_probe) {
        int result = run_fixture_metadata_probe(context, &fixture_model, &model_plan);
        unitlab_free_ied_model_plan(&model_plan);
        unitlab_free_ied_fixture_model(&fixture_model);
        return result;
    }
    if (options->gi_probe) {
        int result = run_fixture_gi_probe(context, &fixture_model, &model_plan);
        unitlab_free_ied_model_plan(&model_plan);
        unitlab_free_ied_fixture_model(&fixture_model);
        return result;
    }
    if (options->native_smoke_start) {
        int result = run_fixture_native_smoke_start(context, &fixture_model, &model_plan);
        unitlab_free_ied_model_plan(&model_plan);
        unitlab_free_ied_fixture_model(&fixture_model);
        return result;
    }
    if (options->native_wire_client_start) {
        int result = run_fixture_native_wire_client_start(context, &fixture_model);
        unitlab_free_ied_model_plan(&model_plan);
        unitlab_free_ied_fixture_model(&fixture_model);
        return result;
    }
    if (options->native_wire_start) {
        int result = run_fixture_native_wire_start(context, &fixture_model, &model_plan);
        unitlab_free_ied_model_plan(&model_plan);
        unitlab_free_ied_fixture_model(&fixture_model);
        return result;
    }
    if (options->smoke_start) {
        int result = run_fixture_server_smoke_start(context, &fixture_model, &model_plan);
        unitlab_free_ied_model_plan(&model_plan);
        unitlab_free_ied_fixture_model(&fixture_model);
        return result;
    }

    int result = run_fixture_server_start(context, &fixture_model, &model_plan);
    unitlab_free_ied_model_plan(&model_plan);
    unitlab_free_ied_fixture_model(&fixture_model);
    return result;
}
