#include "model_loader.h"

#include <stdio.h>
#include <string.h>

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

static UnitLabIedFixtureReport report_for_data_set(const char* data_set_ref)
{
    UnitLabIedFixtureReport report = {
        .key = "IED1/AP1/LD0/LLN0/brcbEvents/buffered",
        .logical_device_inst = "LD0",
        .logical_node_name = "LLN0",
        .report_control_name = "brcbEvents",
        .report_kind = "buffered",
        .rpt_id = "events",
        .conf_rev = "1",
        .indexed_known = 1,
        .indexed = 0,
        .buffer_time_ms_known = 1,
        .buffer_time_ms = 0,
        .integrity_period_ms_known = 1,
        .integrity_period_ms = 0,
    };
    snprintf(report.data_set_ref, sizeof(report.data_set_ref), "%s", data_set_ref);
    return report;
}

static UnitLabIedFixtureModel valid_fixture(
    UnitLabIedFixtureDataSet* data_sets,
    UnitLabIedFixtureReport* reports,
    UnitLabIedFixtureSignal* signals)
{
    signals[0] = (UnitLabIedFixtureSignal){
        .data_set_index = 0U,
        .reference = "LD0/XCBR1.Pos.stVal[ST]",
        .kind = "FCD",
        .fc = "ST",
        .initial_value = "0",
    };
    data_sets[0] = (UnitLabIedFixtureDataSet){
        .reference = "IED1/AP1/LD0/LLN0.dsEvents",
        .signal_count = 1U,
        .signals = signals,
    };
    reports[0] = report_for_data_set("IED1/AP1/LD0/LLN0.dsEvents");
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

static int build_valid_plan(UnitLabIedFixtureModel* fixture, UnitLabIedModelPlan* plan)
{
    char error[256];
    if (!unitlab_build_ied_model_plan(fixture, plan, error, sizeof(error))) {
        fprintf(stderr, "FAIL: model plan should build: %s\n", error);
        return 0;
    }
    return 1;
}

static int test_loader_fails_closed_without_server_backend(void)
{
    UnitLabIedFixtureSignal signals[1];
    UnitLabIedFixtureDataSet data_sets[1];
    UnitLabIedFixtureReport reports[1];
    UnitLabIedFixtureModel fixture = valid_fixture(data_sets, reports, signals);
    UnitLabIedModelPlan plan;
    UnitLabIedModelLoadResult result;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 1102,
    };
    int passed = build_valid_plan(&fixture, &plan);
    if (passed) {
        int loaded = unitlab_load_ied_model(&fixture, &plan, &config, &result);
        passed &= expect_true(!loaded, "loader should fail closed until MMS server backend is implemented");
        passed &= expect_true(result.loaded == 0, "loader result should not report loaded");
#ifdef UNITLAB_WITH_LIBIEC61850
        passed &= expect_string(result.code, "LIBIEC61850_MODEL_LOADER_NOT_IMPLEMENTED", "linked loader failure code");
#else
        passed &= expect_string(result.code, "LIBIEC61850_NOT_LINKED", "unlinked loader failure code");
#endif
    }
    unitlab_free_ied_model_plan(&plan);
    return passed;
}

static int test_loader_rejects_invalid_arguments(void)
{
    UnitLabIedModelLoadResult result;
    int loaded = unitlab_load_ied_model(NULL, NULL, NULL, &result);
    return expect_true(!loaded, "loader should reject null arguments")
        && expect_string(result.code, "MODEL_LOADER_INVALID_ARGUMENT", "invalid argument code");
}

static int test_loader_rejects_empty_plan(void)
{
    UnitLabIedFixtureSignal signals[1];
    UnitLabIedFixtureDataSet data_sets[1];
    UnitLabIedFixtureReport reports[1];
    UnitLabIedFixtureModel fixture = valid_fixture(data_sets, reports, signals);
    UnitLabIedModelPlan plan = {0};
    UnitLabIedModelLoadResult result;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 1102,
    };
    int loaded = unitlab_load_ied_model(&fixture, &plan, &config, &result);
    return expect_true(!loaded, "loader should reject empty model plan")
        && expect_string(result.code, "MODEL_LOADER_EMPTY_PLAN", "empty plan code");
}

static int test_loader_rejects_invalid_bind(void)
{
    UnitLabIedFixtureSignal signals[1];
    UnitLabIedFixtureDataSet data_sets[1];
    UnitLabIedFixtureReport reports[1];
    UnitLabIedFixtureModel fixture = valid_fixture(data_sets, reports, signals);
    UnitLabIedModelPlan plan;
    UnitLabIedModelLoadResult result;
    UnitLabIedServerConfig config = {
        .bind_address = "",
        .port = 1102,
    };
    int passed = build_valid_plan(&fixture, &plan);
    if (passed) {
        int loaded = unitlab_load_ied_model(&fixture, &plan, &config, &result);
        passed &= expect_true(!loaded, "loader should reject empty bind address");
        passed &= expect_string(result.code, "MODEL_LOADER_INVALID_BIND", "invalid bind code");
    }
    unitlab_free_ied_model_plan(&plan);
    return passed;
}

static int test_loader_rejects_invalid_port(void)
{
    UnitLabIedFixtureSignal signals[1];
    UnitLabIedFixtureDataSet data_sets[1];
    UnitLabIedFixtureReport reports[1];
    UnitLabIedFixtureModel fixture = valid_fixture(data_sets, reports, signals);
    UnitLabIedModelPlan plan;
    UnitLabIedModelLoadResult result;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 70000,
    };
    int passed = build_valid_plan(&fixture, &plan);
    if (passed) {
        int loaded = unitlab_load_ied_model(&fixture, &plan, &config, &result);
        passed &= expect_true(!loaded, "loader should reject invalid port");
        passed &= expect_string(result.code, "MODEL_LOADER_INVALID_PORT", "invalid port code");
    }
    unitlab_free_ied_model_plan(&plan);
    return passed;
}

int main(void)
{
    int passed = 1;
    passed &= test_loader_fails_closed_without_server_backend();
    passed &= test_loader_rejects_invalid_arguments();
    passed &= test_loader_rejects_empty_plan();
    passed &= test_loader_rejects_invalid_bind();
    passed &= test_loader_rejects_invalid_port();
    return passed ? 0 : 1;
}
