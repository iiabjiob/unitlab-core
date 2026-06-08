#include "scl_compiler/unitlab_scl_compiler.h"

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

static int test_compile_builds_model_plan_through_c_api(void)
{
    const char* scl =
        "<?xml version=\"1.0\"?>"
        "<scl:SCL xmlns:scl=\"http://www.iec.ch/61850/2003/SCL\">"
        "<scl:IED name=\"IED1\"><scl:AccessPoint name=\"AP1\"><scl:Server><scl:LDevice inst=\"LD0\">"
        "<scl:LN0 lnType=\"LLN0_TYPE\">"
        "<scl:DataSet name=\"dsEvents\">"
        "<scl:FCDA ldInst=\"LD0\" lnClass=\"XCBR\" lnInst=\"1\" doName=\"Pos\" daName=\"stVal\" fc=\"ST\" />"
        "<scl:FCD ldInst=\"LD0\" lnClass=\"PGGIO\" lnInst=\"1\" doName=\"Ind1\" fc=\"ST\" />"
        "</scl:DataSet>"
        "<scl:ReportControl name=\"brcbEvents\" buffered=\"true\" rptID=\"events\" datSet=\"dsEvents\" confRev=\"7\" indexed=\"false\" bufTime=\"100\" intgPd=\"1000\">"
        "<scl:TrgOps dchg=\"true\" qchg=\"true\" dupd=\"false\" period=\"false\" gi=\"true\" />"
        "<scl:OptFields seqNum=\"true\" timeStamp=\"true\" reasonCode=\"true\" dataSet=\"true\" dataRef=\"true\" bufOvfl=\"true\" entryID=\"true\" configRef=\"true\" />"
        "</scl:ReportControl>"
        "</scl:LN0>"
        "<scl:LN lnClass=\"XCBR\" inst=\"1\" lnType=\"XCBR_TYPE\" />"
        "<scl:LN lnClass=\"PGGIO\" inst=\"1\" lnType=\"PGGIO_TYPE\" />"
        "</scl:LDevice></scl:Server></scl:AccessPoint></scl:IED>"
        "<scl:IED name=\"IED2\"><scl:AccessPoint name=\"AP1\" /></scl:IED>"
        "</scl:SCL>";
    UnitLabSclCompileResult* result = NULL;
    char error[128];
    int passed = 1;

    passed &= expect_true(unitlab_scl_compile_from_memory(scl, strlen(scl), "IED1", &result, error, sizeof(error)) == 1,
        "SCL compile should succeed through C API");
    passed &= expect_true(result != NULL, "SCL compile result should be allocated");
    passed &= expect_string(unitlab_scl_compile_selected_ied_name(result), "IED1", "selected IED");
    passed &= expect_true(unitlab_scl_compile_source_size(result) == strlen(scl), "source size should be preserved");
    passed &= expect_true(unitlab_scl_compile_diagnostic_count(result) == 0U, "valid SCL should compile without diagnostics");

    const UnitLabIedModelPlan* plan = unitlab_scl_compile_model_plan(result);
    passed &= expect_true(plan != NULL, "model plan should be available");
    if (plan != NULL) {
        passed &= expect_true(plan->logical_device_count == 1U, "one logical device");
        passed &= expect_true(plan->logical_node_count == 3U, "three logical nodes");
        passed &= expect_true(plan->data_set_count == 1U, "one DataSet");
        passed &= expect_true(plan->report_count == 1U, "one ReportControl");
        passed &= expect_true(plan->signal_count == 2U, "two DataSet members");
        passed &= expect_string(plan->logical_devices[0].inst, "IED1LD0", "MMS domain");
        passed &= expect_string(plan->data_sets[0].reference, "IED1/AP1/LD0/LLN0.dsEvents", "DataSet reference");
        passed &= expect_string(plan->data_sets[0].logical_device_inst, "IED1LD0", "DataSet domain");
        passed &= expect_string(plan->reports[0].name, "brcbEvents", "ReportControl name");
        passed &= expect_string(plan->reports[0].data_set_ref, "IED1/AP1/LD0/LLN0.dsEvents", "ReportControl DatSet ref");
        passed &= expect_true(plan->reports[0].conf_rev_known == 1, "ConfRev should be known");
        passed &= expect_true(plan->reports[0].conf_rev == 7U, "ConfRev value");
        passed &= expect_true(plan->reports[0].buffer_time_ms == 100U, "BufTm value");
        passed &= expect_true(plan->reports[0].integrity_period_ms == 1000U, "IntgPd value");
        passed &= expect_true(
            plan->reports[0].trigger_options_mask
                == (UNITLAB_IED_MODEL_TRG_OPT_DATA_CHANGED | UNITLAB_IED_MODEL_TRG_OPT_QUALITY_CHANGED | UNITLAB_IED_MODEL_TRG_OPT_GI),
            "ReportControl TrgOps mask");
        passed &= expect_true(
            plan->reports[0].optional_fields_mask
                == (UNITLAB_IED_MODEL_RPT_OPT_SEQ_NUM | UNITLAB_IED_MODEL_RPT_OPT_TIME_STAMP
                    | UNITLAB_IED_MODEL_RPT_OPT_REASON_FOR_INCLUSION | UNITLAB_IED_MODEL_RPT_OPT_DATA_SET
                    | UNITLAB_IED_MODEL_RPT_OPT_DATA_REFERENCE | UNITLAB_IED_MODEL_RPT_OPT_BUFFER_OVERFLOW
                    | UNITLAB_IED_MODEL_RPT_OPT_ENTRY_ID | UNITLAB_IED_MODEL_RPT_OPT_CONF_REV),
            "ReportControl OptFlds mask");
        passed &= expect_string(plan->signals[0].reference, "LD0/XCBR1.Pos.stVal[ST]", "first signal ref");
        passed &= expect_string(plan->signals[0].object_reference, "IED1LD0.XCBR1.Pos.stVal", "first signal object ref");
        passed &= expect_string(plan->signals[1].reference, "LD0/PGGIO1.Ind1[ST]", "second signal ref");
        passed &= expect_string(plan->signals[1].object_reference, "IED1LD0.PGGIO1.Ind1", "second signal object ref");
    }

    unitlab_scl_compile_result_free(result);
    return passed;
}

static int test_compile_reports_invalid_selected_ied(void)
{
    const char* scl = "<SCL><IED name=\"IED1\" /></SCL>";
    UnitLabSclCompileResult* result = NULL;
    char error[128];
    int passed = 1;

    passed &= expect_true(unitlab_scl_compile_from_memory(scl, strlen(scl), "MISSING", &result, error, sizeof(error)) == 1,
        "SCL compile should return diagnostics for missing selected IED");
    passed &= expect_true(unitlab_scl_compile_diagnostic_count(result) == 1U, "missing IED error should be present");
    passed &= expect_true(unitlab_scl_compile_model_plan(result) != NULL, "empty model plan should still be readable");
    passed &= expect_true(unitlab_scl_compile_model_plan(result)->logical_device_count == 0U, "missing IED should not compile a model");

    UnitLabSclCompileDiagnostic diagnostic;
    passed &= expect_true(unitlab_scl_compile_diagnostic_at(result, 0U, &diagnostic) == 1, "error diagnostic should be readable");
    passed &= expect_string(diagnostic.severity, "error", "missing IED severity");
    passed &= expect_string(diagnostic.code, "SCL_SELECTED_IED_MISSING", "missing IED code");

    unitlab_scl_compile_result_free(result);
    return passed;
}

static int test_compile_rejects_empty_input(void)
{
    UnitLabSclCompileResult* result = NULL;
    char error[128];
    int passed = 1;

    passed &= expect_true(unitlab_scl_compile_from_memory(NULL, 0U, NULL, &result, error, sizeof(error)) == 0,
        "empty SCL input should fail before allocating result");
    passed &= expect_true(result == NULL, "empty SCL input should not allocate result");
    passed &= expect_string(error, "SCL XML input is required", "empty SCL error");
    return passed;
}

int main(void)
{
    int passed = 1;
    passed &= test_compile_builds_model_plan_through_c_api();
    passed &= test_compile_reports_invalid_selected_ied();
    passed &= test_compile_rejects_empty_input();
    return passed ? 0 : 1;
}
