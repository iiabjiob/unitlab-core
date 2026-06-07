#include "model/model_plan.h"

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

static int starts_with(const char* value, const char* prefix)
{
    return strncmp(value, prefix, strlen(prefix)) == 0;
}

static int expect_list_matches(char** names, size_t count, const char* const* expected, size_t expected_count, const char* message)
{
    if (count != expected_count) {
        fprintf(stderr, "FAIL: %s\n", message);
        return 0;
    }
    for (size_t index = 0U; index < expected_count; index++) {
        if (strcmp(names[index], expected[index]) != 0) {
            fprintf(stderr, "FAIL: %s\n", message);
            return 0;
        }
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
        .buffer_time_ms = 100,
        .integrity_period_ms_known = 1,
        .integrity_period_ms = 1000,
    };
    report.trigger_options.data_change = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 1 };
    report.trigger_options.quality_change = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 0 };
    report.trigger_options.data_update = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 0 };
    report.trigger_options.periodic = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 0 };
    report.trigger_options.general_interrogation = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 1 };
    report.optional_fields.sequence_number = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 1 };
    report.optional_fields.timestamp = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 0 };
    report.optional_fields.reason_code = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 0 };
    report.optional_fields.data_set_name = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 0 };
    report.optional_fields.data_reference = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 1 };
    report.optional_fields.buffer_overflow = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 0 };
    report.optional_fields.entry_id = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 0 };
    report.optional_fields.config_revision = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 0 };
    snprintf(report.data_set_ref, sizeof(report.data_set_ref), "%s", data_set_ref);
    return report;
}

static UnitLabIedFixtureModel fixture_for(
    UnitLabIedFixtureDataSet* data_sets,
    size_t data_set_count,
    UnitLabIedFixtureReport* reports,
    size_t report_count,
    size_t signal_count)
{
    UnitLabIedFixtureModel fixture = {
        .device_count = 1U,
        .ied_name = "IED1",
        .access_point_name = "AP1",
        .data_set_count = data_set_count,
        .data_sets = data_sets,
        .report_count = report_count,
        .reports = reports,
        .signal_count = signal_count,
    };
    return fixture;
}

static int test_model_plan_builds_blueprint(void)
{
    UnitLabIedFixtureSignal signals[2] = {
        {
            .data_set_index = 0U,
            .reference = "LD0/XCBR1.Pos.stVal[ST]",
            .kind = "FCDA",
            .component = "phaseA",
            .fc = "ST",
            .initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_INTEGER,
            .initial_value = "0",
        },
        {
            .data_set_index = 1U,
            .reference = "LD0/PGGIO1.Ind1[ST]",
            .kind = "FCD",
            .fc = "ST",
            .initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_INTEGER,
            .initial_value = "1",
        },
    };
    UnitLabIedFixtureDataSet data_sets[1] = {
        {
            .reference = "IED1/AP1/LD0/LLN0.dsEvents",
            .signal_count = 2U,
            .signals = signals,
        },
    };
    UnitLabIedFixtureReport reports[1] = {
        report_for_data_set("IED1/AP1/LD0/LLN0.dsEvents"),
    };
    UnitLabIedFixtureModel fixture = fixture_for(data_sets, 1U, reports, 1U, 2U);
    UnitLabIedModelPlan plan;
    char error[256];
    int ok = unitlab_build_ied_model_plan(&fixture, &plan, error, sizeof(error));
    int passed = 1;
    passed &= expect_true(ok, "model plan should build");
    if (ok) {
        passed &= expect_true(plan.logical_device_count == 1U, "one logical device");
        passed &= expect_true(plan.logical_node_count == 3U, "three logical nodes");
        passed &= expect_true(plan.data_set_count == 1U, "one DataSet");
        passed &= expect_true(plan.report_count == 1U, "one ReportControl");
        passed &= expect_true(plan.signal_count == 2U, "two signals");
        passed &= expect_true(plan.namespace_attribute_count == 4U, "four namespace attributes");
        if (plan.namespace_attribute_count == 4U) {
            passed &= expect_string(plan.namespace_attributes[0].object_reference, "LD0.LLN0.EX.NamPlt.ldNs", "namespace ldNs object reference");
            passed &= expect_string(plan.namespace_attributes[0].initial_value, "LD0", "namespace ldNs value");
            passed &= expect_string(plan.namespace_attributes[1].object_reference, "LD0.LLN0.EX.NamPlt.lnNs", "namespace lnNs object reference");
            passed &= expect_string(plan.namespace_attributes[1].initial_value, "IEC 61850-7-4:2007", "namespace lnNs value");
            passed &= expect_string(plan.namespace_attributes[2].object_reference, "LD0.LLN0.EX.NamPlt.cdcNs", "namespace cdcNs object reference");
            passed &= expect_string(plan.namespace_attributes[2].initial_value, "IEC 61850-7-3:2010", "namespace cdcNs value");
            passed &= expect_string(plan.namespace_attributes[3].object_reference, "LD0.LLN0.EX.NamPlt.dataNs", "namespace dataNs object reference");
            passed &= expect_string(plan.namespace_attributes[3].initial_value, "EXT:2015", "namespace dataNs value");
        }
        passed &= expect_string(plan.data_sets[0].logical_device_inst, "LD0", "DataSet LD");
        passed &= expect_string(plan.data_sets[0].logical_node_name, "LLN0", "DataSet LN");
        passed &= expect_string(plan.data_sets[0].name, "dsEvents", "DataSet name");
        passed &= expect_string(plan.reports[0].name, "brcbEvents", "ReportControl name");
        passed &= expect_string(plan.reports[0].report_kind, "buffered", "ReportControl kind");
        passed &= expect_true(plan.reports[0].is_buffered == 1, "ReportControl buffered flag");
        passed &= expect_string(plan.reports[0].rpt_id, "events", "ReportControl rptID");
        passed &= expect_true(plan.reports[0].conf_rev_known == 1, "ReportControl ConfRev known");
        passed &= expect_true(plan.reports[0].conf_rev == 1U, "ReportControl ConfRev");
        passed &= expect_true(plan.reports[0].indexed_known == 1, "ReportControl indexed known");
        passed &= expect_true(plan.reports[0].indexed == 0, "ReportControl indexed value");
        passed &= expect_true(plan.reports[0].buffer_time_ms_known == 1, "ReportControl BufTm known");
        passed &= expect_true(plan.reports[0].buffer_time_ms == 100U, "ReportControl BufTm");
        passed &= expect_true(plan.reports[0].integrity_period_ms_known == 1, "ReportControl IntgPd known");
        passed &= expect_true(plan.reports[0].integrity_period_ms == 1000U, "ReportControl IntgPd");
        passed &= expect_true(plan.reports[0].trigger_options.data_change.known == 1, "ReportControl dchg known");
        passed &= expect_true(plan.reports[0].trigger_options.data_change.value == 1, "ReportControl dchg value");
        passed &= expect_true(plan.reports[0].optional_fields.data_reference.known == 1, "ReportControl dataRef known");
        passed &= expect_true(plan.reports[0].optional_fields.data_reference.value == 1, "ReportControl dataRef value");
        passed &= expect_true(
            plan.reports[0].trigger_options_mask
                == (UNITLAB_IED_MODEL_TRG_OPT_DATA_CHANGED | UNITLAB_IED_MODEL_TRG_OPT_GI),
            "ReportControl TrgOps mask");
        passed &= expect_true(
            plan.reports[0].optional_fields_mask
                == (UNITLAB_IED_MODEL_RPT_OPT_SEQ_NUM | UNITLAB_IED_MODEL_RPT_OPT_DATA_REFERENCE),
            "ReportControl OptFlds mask");
        passed &= expect_string(plan.signals[0].logical_node_name, "XCBR1", "first signal LN");
        passed &= expect_string(plan.signals[0].kind, "FCDA", "first signal kind");
        passed &= expect_string(plan.signals[0].object_reference, "Pos.stVal", "first signal object reference");
        passed &= expect_string(plan.signals[0].data_object_name, "Pos", "first signal data object");
        passed &= expect_string(plan.signals[0].data_attribute_path, "stVal", "first signal data attribute");
        passed &= expect_string(
            plan.signals[0].data_set_entry_variable,
            "LD0/XCBR1$ST$Pos$stVal",
            "first signal DataSetEntry variable");
        passed &= expect_true(plan.signals[0].data_set_entry_component_known == 1, "first signal DataSetEntry component known");
        passed &= expect_string(plan.signals[0].data_set_entry_component, "phaseA", "first signal DataSetEntry component");
        passed &= expect_string(plan.signals[0].fc, "ST", "first signal FC");
        passed &= expect_true(plan.signals[0].initial_value_kind == UNITLAB_IED_FIXTURE_VALUE_INTEGER, "first signal value kind");
        passed &= expect_string(plan.signals[0].initial_value, "0", "first signal initial value");
        passed &= expect_string(plan.signals[1].kind, "FCD", "second signal kind");
        passed &= expect_string(plan.signals[1].object_reference, "Ind1", "second signal FCD parent object reference");
        passed &= expect_string(plan.signals[1].data_object_name, "Ind1", "second signal data object");
        passed &= expect_string(plan.signals[1].data_attribute_path, "", "second signal data attribute");
        passed &= expect_string(
            plan.signals[1].data_set_entry_variable,
            "LD0/PGGIO1$ST$Ind1",
            "second signal DataSetEntry variable");
        passed &= expect_true(plan.signals[1].initial_value_kind == UNITLAB_IED_FIXTURE_VALUE_INTEGER, "second signal value kind");
    }
    unitlab_free_ied_model_plan(&plan);
    return passed;
}

static int test_collects_named_variables_for_domain_browse(void)
{
    UnitLabIedModelLogicalDevice logical_devices[1] = {
        { .inst = "LD0" },
    };
    UnitLabIedModelLogicalNode logical_nodes[3] = {
        { .logical_device_inst = "LD0", .name = "LLN0" },
        { .logical_device_inst = "LD0", .name = "XCBR1" },
        { .logical_device_inst = "LD0", .name = "PGGIO1" },
    };
    UnitLabIedModelSignal signals[2] = {
        {
            .logical_device_inst = "LD0",
            .logical_node_name = "XCBR1",
            .data_set_entry_variable = "LD0/XCBR1$ST$Pos$stVal",
        },
        {
            .logical_device_inst = "LD0",
            .logical_node_name = "PGGIO1",
            .data_set_entry_variable = "LD0/PGGIO1$ST$Ind1",
        },
    };
    UnitLabIedModelPlan plan = {
        .logical_device_count = 1U,
        .logical_devices = logical_devices,
        .logical_node_count = 3U,
        .logical_nodes = logical_nodes,
        .signal_count = 2U,
        .signals = signals,
    };
    char error[256];
    char** names = NULL;
    size_t count = 0U;
    int passed = 1;

    passed &= expect_true(
        unitlab_collect_ied_model_logical_device_variables(&plan, "LD0", &names, &count, error, sizeof(error)) == 1,
        "domain browse should collect named variables");
    if (passed) {
        passed &= expect_list_matches(
            names,
            count,
            (const char*[]){ "LLN0", "XCBR1", "PGGIO1" },
            3U,
            "domain browse should collect only top-level logical nodes");
    }

    unitlab_free_ied_model_name_list(names, count);
    return passed;
}

static int test_collects_logical_node_namespace_attributes(void)
{
    UnitLabIedFixtureSignal signals[1] = {
        {
            .data_set_index = 0U,
            .reference = "LD0/XCBR1.Pos.stVal[ST]",
            .kind = "FCDA",
            .fc = "ST",
            .initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_INTEGER,
            .initial_value = "0",
        },
    };
    UnitLabIedFixtureDataSet data_sets[1] = {
        {
            .reference = "IED1/AP1/LD0/LLN0.dsEvents",
            .signal_count = 1U,
            .signals = signals,
        },
    };
    UnitLabIedFixtureReport reports[1] = {
        report_for_data_set("IED1/AP1/LD0/LLN0.dsEvents"),
    };
    UnitLabIedFixtureModel fixture = fixture_for(data_sets, 1U, reports, 1U, 1U);
    UnitLabIedModelPlan plan;
    char error[256];
    char** names = NULL;
    size_t count = 0U;
    int passed = 1;

    passed &= expect_true(unitlab_build_ied_model_plan(&fixture, &plan, error, sizeof(error)) == 1, "namespace model plan should build");
    if (passed) {
        passed &= expect_true(
            unitlab_collect_ied_model_logical_node_namespace_attributes(&plan, "LD0", "LLN0", &names, &count, error, sizeof(error)) == 1,
            "namespace browse should collect namespace attributes");
        if (passed) {
            passed &= expect_list_matches(names, count, (const char*[]){ "ldNs", "lnNs", "cdcNs", "dataNs" }, 4U, "LLN0 should expose namespace attributes");
        }
    }
    unitlab_free_ied_model_name_list(names, count);
    unitlab_free_ied_model_plan(&plan);
    return passed;
}

static int test_collects_logical_node_variables_for_directory_browse(void)
{
    UnitLabIedModelLogicalDevice logical_devices[1] = {
        { .inst = "LD0" },
    };
    UnitLabIedModelLogicalNode logical_nodes[2] = {
        { .logical_device_inst = "LD0", .name = "LLN0" },
        { .logical_device_inst = "LD0", .name = "XCBR1" },
    };
    UnitLabIedModelSignal signals[2] = {
        {
            .logical_device_inst = "LD0",
            .logical_node_name = "XCBR1",
            .data_object_name = "Pos",
        },
        {
            .logical_device_inst = "LD0",
            .logical_node_name = "XCBR1",
            .data_object_name = "Loc",
        },
    };
    UnitLabIedModelPlan plan = {
        .logical_device_count = 1U,
        .logical_devices = logical_devices,
        .logical_node_count = 2U,
        .logical_nodes = logical_nodes,
        .signal_count = 2U,
        .signals = signals,
    };
    char error[256];
    char** names = NULL;
    size_t count = 0U;
    int passed = 1;

    passed &= expect_true(
        unitlab_collect_ied_model_logical_node_variables(&plan, "LD0", "LLN0", &names, &count, error, sizeof(error)) == 1,
        "logical-node browse should collect common variables");
    if (passed) {
        passed &= expect_list_matches(
            names,
            count,
            (const char*[]){ "Mod", "Beh", "Health", "CF", "DC", "BR", "EX" },
            7U,
            "LLN0 should expose standard common variables");
    }
    unitlab_free_ied_model_name_list(names, count);
    names = NULL;
    count = 0U;

    passed &= expect_true(
        unitlab_collect_ied_model_logical_node_variables(&plan, "LD0", "XCBR1", &names, &count, error, sizeof(error)) == 1,
        "logical-node browse should collect data-object names");
    if (passed) {
        passed &= expect_list_matches(names, count, (const char*[]){ "Pos", "Loc" }, 2U, "XCBR1 should expose signal data objects");
    }
    unitlab_free_ied_model_name_list(names, count);
    return passed;
}

static int test_missing_report_dataset_fails(void)
{
    UnitLabIedFixtureSignal signals[1] = {
        {
            .data_set_index = 0U,
            .reference = "LD0/XCBR1.Pos.stVal[ST]",
            .kind = "FCD",
            .fc = "ST",
            .initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_INTEGER,
            .initial_value = "0",
        },
    };
    UnitLabIedFixtureDataSet data_sets[1] = {
        {
            .reference = "IED1/AP1/LD0/LLN0.dsEvents",
            .signal_count = 1U,
            .signals = signals,
        },
    };
    UnitLabIedFixtureReport reports[1] = {
        report_for_data_set("IED1/AP1/LD0/LLN0.missing"),
    };
    UnitLabIedFixtureModel fixture = fixture_for(data_sets, 1U, reports, 1U, 1U);
    UnitLabIedModelPlan plan;
    char error[256];
    int ok = unitlab_build_ied_model_plan(&fixture, &plan, error, sizeof(error));
    unitlab_free_ied_model_plan(&plan);
    return expect_true(!ok, "missing report DataSet should fail")
        && expect_true(starts_with(error, "MODEL_PLAN_REPORT_DATASET_NOT_FOUND"), "missing DataSet error code");
}

static int test_invalid_signal_reference_fails(void)
{
    UnitLabIedFixtureSignal signals[1] = {
        {
            .data_set_index = 0U,
            .reference = "LD0/XCBR1[ST]",
            .kind = "FCD",
            .fc = "ST",
            .initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_INTEGER,
            .initial_value = "0",
        },
    };
    UnitLabIedFixtureDataSet data_sets[1] = {
        {
            .reference = "IED1/AP1/LD0/LLN0.dsEvents",
            .signal_count = 1U,
            .signals = signals,
        },
    };
    UnitLabIedFixtureReport reports[1] = {
        report_for_data_set("IED1/AP1/LD0/LLN0.dsEvents"),
    };
    UnitLabIedFixtureModel fixture = fixture_for(data_sets, 1U, reports, 1U, 1U);
    UnitLabIedModelPlan plan;
    char error[256];
    int ok = unitlab_build_ied_model_plan(&fixture, &plan, error, sizeof(error));
    unitlab_free_ied_model_plan(&plan);
    return expect_true(!ok, "invalid signal reference should fail")
        && expect_true(starts_with(error, "MODEL_PLAN_SIGNAL_REFERENCE_INVALID"), "invalid signal error code");
}

static int test_signal_fc_mismatch_fails(void)
{
    UnitLabIedFixtureSignal signals[1] = {
        {
            .data_set_index = 0U,
            .reference = "LD0/XCBR1.Pos.stVal[ST]",
            .kind = "FCD",
            .fc = "MX",
            .initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_INTEGER,
            .initial_value = "0",
        },
    };
    UnitLabIedFixtureDataSet data_sets[1] = {
        {
            .reference = "IED1/AP1/LD0/LLN0.dsEvents",
            .signal_count = 1U,
            .signals = signals,
        },
    };
    UnitLabIedFixtureReport reports[1] = {
        report_for_data_set("IED1/AP1/LD0/LLN0.dsEvents"),
    };
    UnitLabIedFixtureModel fixture = fixture_for(data_sets, 1U, reports, 1U, 1U);
    UnitLabIedModelPlan plan;
    char error[256];
    int ok = unitlab_build_ied_model_plan(&fixture, &plan, error, sizeof(error));
    unitlab_free_ied_model_plan(&plan);
    return expect_true(!ok, "signal FC mismatch should fail")
        && expect_true(starts_with(error, "MODEL_PLAN_SIGNAL_FC_MISMATCH"), "FC mismatch error code");
}

static int test_invalid_signal_kind_fails(void)
{
    UnitLabIedFixtureSignal signals[1] = {
        {
            .data_set_index = 0U,
            .reference = "LD0/XCBR1.Pos.stVal[ST]",
            .kind = "BAD",
            .fc = "ST",
            .initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_INTEGER,
            .initial_value = "0",
        },
    };
    UnitLabIedFixtureDataSet data_sets[1] = {
        {
            .reference = "IED1/AP1/LD0/LLN0.dsEvents",
            .signal_count = 1U,
            .signals = signals,
        },
    };
    UnitLabIedFixtureReport reports[1] = {
        report_for_data_set("IED1/AP1/LD0/LLN0.dsEvents"),
    };
    UnitLabIedFixtureModel fixture = fixture_for(data_sets, 1U, reports, 1U, 1U);
    UnitLabIedModelPlan plan;
    char error[256];
    int ok = unitlab_build_ied_model_plan(&fixture, &plan, error, sizeof(error));
    unitlab_free_ied_model_plan(&plan);
    return expect_true(!ok, "invalid signal kind should fail")
        && expect_true(starts_with(error, "MODEL_PLAN_SIGNAL_KIND_INVALID"), "invalid signal kind error code");
}

static int test_invalid_report_kind_fails(void)
{
    UnitLabIedFixtureSignal signals[1] = {
        {
            .data_set_index = 0U,
            .reference = "LD0/XCBR1.Pos.stVal[ST]",
            .kind = "FCDA",
            .fc = "ST",
            .initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_INTEGER,
            .initial_value = "0",
        },
    };
    UnitLabIedFixtureDataSet data_sets[1] = {
        {
            .reference = "IED1/AP1/LD0/LLN0.dsEvents",
            .signal_count = 1U,
            .signals = signals,
        },
    };
    UnitLabIedFixtureReport reports[1] = {
        report_for_data_set("IED1/AP1/LD0/LLN0.dsEvents"),
    };
    snprintf(reports[0].report_kind, sizeof(reports[0].report_kind), "%s", "unknown");
    UnitLabIedFixtureModel fixture = fixture_for(data_sets, 1U, reports, 1U, 1U);
    UnitLabIedModelPlan plan;
    char error[256];
    int ok = unitlab_build_ied_model_plan(&fixture, &plan, error, sizeof(error));
    unitlab_free_ied_model_plan(&plan);
    return expect_true(!ok, "invalid report kind should fail")
        && expect_true(starts_with(error, "MODEL_PLAN_REPORT_KIND_INVALID"), "invalid report kind error code");
}

static int test_invalid_report_conf_rev_fails(void)
{
    UnitLabIedFixtureSignal signals[1] = {
        {
            .data_set_index = 0U,
            .reference = "LD0/XCBR1.Pos.stVal[ST]",
            .kind = "FCDA",
            .fc = "ST",
            .initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_INTEGER,
            .initial_value = "0",
        },
    };
    UnitLabIedFixtureDataSet data_sets[1] = {
        {
            .reference = "IED1/AP1/LD0/LLN0.dsEvents",
            .signal_count = 1U,
            .signals = signals,
        },
    };
    UnitLabIedFixtureReport reports[1] = {
        report_for_data_set("IED1/AP1/LD0/LLN0.dsEvents"),
    };
    snprintf(reports[0].conf_rev, sizeof(reports[0].conf_rev), "%s", "bad");
    UnitLabIedFixtureModel fixture = fixture_for(data_sets, 1U, reports, 1U, 1U);
    UnitLabIedModelPlan plan;
    char error[256];
    int ok = unitlab_build_ied_model_plan(&fixture, &plan, error, sizeof(error));
    unitlab_free_ied_model_plan(&plan);
    return expect_true(!ok, "invalid report ConfRev should fail")
        && expect_true(starts_with(error, "MODEL_PLAN_REPORT_CONFREV_INVALID"), "invalid ConfRev error code");
}

static int test_unknown_initial_value_kind_fails(void)
{
    UnitLabIedFixtureSignal signals[1] = {
        {
            .data_set_index = 0U,
            .reference = "LD0/XCBR1.Pos.stVal[ST]",
            .kind = "FCDA",
            .fc = "ST",
            .initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_UNKNOWN,
            .initial_value = "0",
        },
    };
    UnitLabIedFixtureDataSet data_sets[1] = {
        {
            .reference = "IED1/AP1/LD0/LLN0.dsEvents",
            .signal_count = 1U,
            .signals = signals,
        },
    };
    UnitLabIedFixtureReport reports[1] = {
        report_for_data_set("IED1/AP1/LD0/LLN0.dsEvents"),
    };
    UnitLabIedFixtureModel fixture = fixture_for(data_sets, 1U, reports, 1U, 1U);
    UnitLabIedModelPlan plan;
    char error[256];
    int ok = unitlab_build_ied_model_plan(&fixture, &plan, error, sizeof(error));
    unitlab_free_ied_model_plan(&plan);
    return expect_true(!ok, "unknown value kind should fail")
        && expect_true(starts_with(error, "MODEL_PLAN_SIGNAL_VALUE_KIND_UNKNOWN"), "unknown value kind error code");
}


static int test_metadata_catalog(void)
{
    UnitLabIedFixtureSignal signals[3];
    UnitLabIedFixtureDataSet data_sets[2];
    UnitLabIedFixtureReport reports[2];
    UnitLabIedFixtureModel fixture = fixture_for(data_sets, 2U, reports, 1U, 2U);
    UnitLabIedModelPlan plan;
    char error[256];
    char** names = NULL;
    size_t count = 0U;
    int passed = 1;

    signals[0] = (UnitLabIedFixtureSignal){
        .data_set_index = 0U,
        .reference = "LD0/XCBR1.Pos.stVal[ST]",
        .kind = "FCDA",
        .component = "phaseA",
        .fc = "ST",
        .initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_INTEGER,
        .initial_value = "0",
    };
    signals[1] = (UnitLabIedFixtureSignal){
        .data_set_index = 0U,
        .reference = "LD0/PGGIO1.Ind1[ST]",
        .kind = "FCD",
        .fc = "ST",
        .initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_INTEGER,
        .initial_value = "1",
    };
    data_sets[0] = (UnitLabIedFixtureDataSet){
        .reference = "IED1/AP1/LD0/LLN0.dsEvents",
        .signal_count = 1U,
        .signals = &signals[0],
    };
    data_sets[1] = (UnitLabIedFixtureDataSet){
        .reference = "IED1/AP1/LD0/LLN0.dsUpdates",
        .signal_count = 1U,
        .signals = &signals[1],
    };

    reports[0] = report_for_data_set("IED1/AP1/LD0/LLN0.dsEvents");

    if (!unitlab_build_ied_model_plan(&fixture, &plan, error, sizeof(error))) {
        fprintf(stderr, "FAIL: model plan should build: %s\n", error);
        return 0;
    }

    int ok = unitlab_collect_ied_model_logical_devices(&plan, &names, &count, error, sizeof(error));
    passed &= ok;
    if (ok) {
        passed &= expect_list_matches(names, count, (const char*[]){ "LD0" }, 1U, "logical device metadata");
    }
    unitlab_free_ied_model_name_list(names, count);
    names = NULL;
    count = 0U;

    ok = unitlab_collect_ied_model_logical_node_data_sets(&plan, "LD0", "LLN0", &names, &count, error, sizeof(error));
    passed &= ok;
    if (ok) {
        passed &= expect_list_matches(names, count, (const char*[]){ "dsEvents", "dsUpdates" }, 2U, "logical node data sets");
    }
    unitlab_free_ied_model_name_list(names, count);
    names = NULL;
    count = 0U;

    ok = unitlab_collect_ied_model_logical_device_data_sets(&plan, "LD0", &names, &count, error, sizeof(error));
    passed &= ok;
    if (ok) {
        passed &= expect_list_matches(names, count, (const char*[]){ "LLN0$dsEvents", "LLN0$dsUpdates" }, 2U, "logical device data sets");
    }
    unitlab_free_ied_model_name_list(names, count);
    names = NULL;
    count = 0U;

    ok = unitlab_collect_ied_model_logical_node_reports(
        &plan,
        "LD0",
        "LLN0",
        UNITLAB_IED_MODEL_REPORT_CONTROL_KIND_BUFFERED,
        &names,
        &count,
        error,
        sizeof(error));
    passed &= ok;
    if (ok) {
        passed &= expect_list_matches(names, count, (const char*[]){ "brcbEvents" }, 1U, "logical node buffered reports");
    }
    unitlab_free_ied_model_name_list(names, count);

    unitlab_free_ied_model_plan(&plan);
    return passed;
}

static int test_dataset_context_mismatch_fails(void)
{
    UnitLabIedFixtureSignal signals[1] = {
        {
            .data_set_index = 0U,
            .reference = "LD0/XCBR1.Pos.stVal[ST]",
            .kind = "FCD",
            .fc = "ST",
            .initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_INTEGER,
            .initial_value = "0",
        },
    };
    UnitLabIedFixtureDataSet data_sets[1] = {
        {
            .reference = "OTHER/AP1/LD0/LLN0.dsEvents",
            .signal_count = 1U,
            .signals = signals,
        },
    };
    UnitLabIedFixtureReport reports[1] = {
        report_for_data_set("OTHER/AP1/LD0/LLN0.dsEvents"),
    };
    UnitLabIedFixtureModel fixture = fixture_for(data_sets, 1U, reports, 1U, 1U);
    UnitLabIedModelPlan plan;
    char error[256];
    int ok = unitlab_build_ied_model_plan(&fixture, &plan, error, sizeof(error));
    unitlab_free_ied_model_plan(&plan);
    return expect_true(!ok, "DataSet IED/AP context mismatch should fail")
        && expect_true(starts_with(error, "MODEL_PLAN_DATASET_CONTEXT_MISMATCH"), "DataSet context mismatch error code");
}

int main(void)
{
    int passed = 1;
    passed &= test_model_plan_builds_blueprint();
    passed &= test_collects_named_variables_for_domain_browse();
    passed &= test_collects_logical_node_namespace_attributes();
    passed &= test_collects_logical_node_variables_for_directory_browse();
    passed &= test_missing_report_dataset_fails();
    passed &= test_invalid_signal_reference_fails();
    passed &= test_signal_fc_mismatch_fails();
    passed &= test_invalid_signal_kind_fails();
    passed &= test_invalid_report_kind_fails();
    passed &= test_invalid_report_conf_rev_fails();
    passed &= test_unknown_initial_value_kind_fails();
    passed &= test_dataset_context_mismatch_fails();
    passed &= test_metadata_catalog();
    return passed ? 0 : 1;
}
