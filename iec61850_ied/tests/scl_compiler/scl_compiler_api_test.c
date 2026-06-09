#include "scl_compiler/unitlab_scl_compiler.h"

#include <stdio.h>
#include <stdlib.h>
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

static int expect_contains(const char* actual, const char* expected, const char* message)
{
    if (strstr(actual, expected) == NULL) {
        fprintf(stderr, "FAIL: %s: expected substring \"%s\" in JSON\n", message, expected);
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
        "<scl:FCDA ldInst=\"LD0\" lnClass=\"XCBR\" lnInst=\"1\" doName=\"Pos\" daName=\"origin.orIdent\" fc=\"ST\" />"
        "<scl:FCDA ldInst=\"LD0\" lnClass=\"XCBR\" lnInst=\"1\" doName=\"Pos\" daName=\"origin.nested.deepIdent\" fc=\"ST\" />"
        "<scl:FCDA ldInst=\"LD0\" lnClass=\"XCBR\" lnInst=\"1\" doName=\"Pos\" daName=\"ctlModel\" fc=\"CF\" />"
        "<scl:FCDA ldInst=\"LD0\" lnClass=\"XCBR\" lnInst=\"1\" doName=\"Beh.subState\" daName=\"stVal\" fc=\"ST\" />"
        "<scl:FCD ldInst=\"LD0\" lnClass=\"PGGIO\" lnInst=\"1\" doName=\"Ind1\" fc=\"ST\" />"
        "<scl:FCDA ldInst=\"LD0\" prefix=\"Led\" lnClass=\"GGIO\" lnInst=\"1\" doName=\"Ind1\" fc=\"ST\" />"
        "<scl:FCDA ldInst=\"LD1\" lnClass=\"PGGIO\" lnInst=\"1\" doName=\"Ind1\" fc=\"ST\" />"
        "<scl:FCD ldInst=\"LD0\" lnClass=\"MMXU\" lnInst=\"1\" doName=\"PhV.phsA\" fc=\"MX\" />"
        "<scl:FCD ldInst=\"LD0\" lnClass=\"MMXU\" lnInst=\"1\" doName=\"Hz\" fc=\"MX\" />"
        "</scl:DataSet>"
        "<scl:ReportControl name=\"brcbEvents\" buffered=\"true\" rptID=\"events\" datSet=\"dsEvents\" confRev=\"7\" indexed=\"false\" bufTime=\"100\" intgPd=\"1000\">"
        "<scl:TrgOps dchg=\"true\" qchg=\"true\" dupd=\"false\" period=\"false\" gi=\"true\" />"
        "<scl:OptFields seqNum=\"true\" timeStamp=\"true\" reasonCode=\"true\" dataSet=\"true\" dataRef=\"true\" bufOvfl=\"true\" entryID=\"true\" configRef=\"true\" />"
        "</scl:ReportControl>"
        "<scl:ReportControl name=\"urcbEvents\" buffered=\"false\" rptID=\"eventsU\" datSet=\"dsEvents\" confRev=\"8\" indexed=\"false\">"
        "<scl:TrgOps dchg=\"true\" qchg=\"false\" dupd=\"false\" period=\"false\" gi=\"true\" />"
        "<scl:OptFields seqNum=\"true\" timeStamp=\"true\" reasonCode=\"true\" dataSet=\"true\" dataRef=\"true\" />"
        "</scl:ReportControl>"
        "</scl:LN0>"
        "<scl:LN lnClass=\"XCBR\" inst=\"1\" lnType=\"XCBR_TYPE\" />"
        "<scl:LN lnClass=\"PGGIO\" inst=\"1\" lnType=\"PGGIO_TYPE\" />"
        "<scl:LN prefix=\"Led\" lnClass=\"GGIO\" inst=\"1\" lnType=\"PGGIO_TYPE\" />"
        "<scl:LN lnClass=\"MMXU\" inst=\"1\" lnType=\"MMXU_TYPE\" />"
        "</scl:LDevice><scl:LDevice inst=\"LD1\"><scl:LN lnClass=\"PGGIO\" inst=\"1\" lnType=\"PGGIO_TYPE\" /></scl:LDevice></scl:Server></scl:AccessPoint></scl:IED>"
        "<scl:IED name=\"IED2\"><scl:AccessPoint name=\"AP1\" /></scl:IED>"
        "<scl:DataTypeTemplates>"
        "<scl:LNodeType id=\"XCBR_TYPE\" lnClass=\"XCBR\"><scl:DO name=\"Pos\" type=\"DPC_POS\" /><scl:DO name=\"Beh\" type=\"BEH_ROOT\" /></scl:LNodeType>"
        "<scl:LNodeType id=\"PGGIO_TYPE\" lnClass=\"PGGIO\"><scl:DO name=\"Ind1\" type=\"INS_IND\" /></scl:LNodeType>"
        "<scl:LNodeType id=\"MMXU_TYPE\" lnClass=\"MMXU\"><scl:DO name=\"PhV\" type=\"PHV_ROOT\" /><scl:DO name=\"Hz\" type=\"MV_ROOT\" /></scl:LNodeType>"
        "<scl:DOType id=\"DPC_POS\" cdc=\"DPC\"><scl:DA name=\"stVal\" fc=\"ST\" bType=\"BOOLEAN\" /><scl:DA name=\"origin\" fc=\"ST\" bType=\"Struct\" type=\"ORIGINATOR\" /><scl:DA name=\"ctlModel\" fc=\"CF\" bType=\"Enum\" type=\"CtlModelKind\" /></scl:DOType>"
        "<scl:DOType id=\"BEH_ROOT\" cdc=\"ENS\"><scl:SDO name=\"subState\" type=\"BEH_SUB\" /></scl:DOType>"
        "<scl:DOType id=\"BEH_SUB\" cdc=\"ENS\"><scl:DA name=\"stVal\" fc=\"ST\" bType=\"INT32\" /></scl:DOType>"
        "<scl:DOType id=\"INS_IND\" cdc=\"INS\"><scl:DA name=\"stVal\" fc=\"ST\" bType=\"INT32\" /><scl:DA name=\"q\" fc=\"ST\" bType=\"Quality\" /><scl:DA name=\"t\" fc=\"ST\" bType=\"Timestamp\" /></scl:DOType>"
        "<scl:DOType id=\"PHV_ROOT\" cdc=\"WYE\"><scl:SDO name=\"phsA\" type=\"CMV_ROOT\" /></scl:DOType>"
        "<scl:DOType id=\"CMV_ROOT\" cdc=\"CMV\"><scl:DA name=\"cVal\" fc=\"MX\" bType=\"Struct\" type=\"Vector\" /><scl:DA name=\"q\" fc=\"MX\" bType=\"Quality\" /><scl:DA name=\"t\" fc=\"MX\" bType=\"Timestamp\" /></scl:DOType>"
        "<scl:DOType id=\"MV_ROOT\" cdc=\"MV\"><scl:DA name=\"mag\" fc=\"MX\" bType=\"Struct\" type=\"AnalogueValue\" /><scl:DA name=\"q\" fc=\"MX\" bType=\"Quality\" /><scl:DA name=\"t\" fc=\"MX\" bType=\"Timestamp\" /></scl:DOType>"
        "<scl:DAType id=\"ORIGINATOR\"><scl:BDA name=\"orIdent\" bType=\"VisString64\" /><scl:BDA name=\"nested\" bType=\"Struct\" type=\"ORIGINATOR_NESTED\" /></scl:DAType>"
        "<scl:DAType id=\"Vector\"><scl:BDA name=\"mag\" bType=\"Struct\" type=\"AnalogueValue\" /></scl:DAType>"
        "<scl:DAType id=\"AnalogueValue\"><scl:BDA name=\"f\" bType=\"FLOAT32\" /></scl:DAType>"
        "<scl:DAType id=\"ORIGINATOR_NESTED\"><scl:BDA name=\"deepIdent\" bType=\"VisString64\" /></scl:DAType>"
        "<scl:EnumType id=\"CtlModelKind\"><scl:EnumVal ord=\"1\" desc=\"direct-with-normal-security\" /></scl:EnumType>"
        "</scl:DataTypeTemplates>"
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
        passed &= expect_true(plan->logical_device_count == 2U, "two logical devices including cross-LD member target");
        passed &= expect_true(plan->logical_node_count == 6U, "six logical nodes including prefixed, cross-LD, and MMXU LN");
        passed &= expect_true(plan->data_set_count == 1U, "one DataSet");
        passed &= expect_true(plan->report_count == 2U, "buffered and unbuffered ReportControls");
        passed &= expect_true(plan->signal_count == 20U, "twenty DataSet members including recursive CMV/MV leaves");
        passed &= expect_string(plan->logical_devices[0].inst, "IED1LD0", "MMS domain");
        passed &= expect_string(plan->data_sets[0].reference, "IED1/AP1/LD0/LLN0.dsEvents", "DataSet reference");
        passed &= expect_string(plan->data_sets[0].logical_device_inst, "IED1LD0", "DataSet domain");
        passed &= expect_string(plan->reports[0].name, "brcbEvents", "ReportControl name");
        passed &= expect_string(plan->reports[0].data_set_ref, "IED1/AP1/LD0/LLN0.dsEvents", "ReportControl DatSet ref");
        passed &= expect_true(plan->reports[0].is_buffered == 1, "buffered ReportControl flag");
        passed &= expect_string(plan->reports[0].report_kind, "buffered", "buffered ReportControl kind");
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
        passed &= expect_string(plan->reports[1].name, "urcbEvents", "unbuffered ReportControl name");
        passed &= expect_true(plan->reports[1].is_buffered == 0, "unbuffered ReportControl flag");
        passed &= expect_string(plan->reports[1].report_kind, "unbuffered", "unbuffered ReportControl kind");
        passed &= expect_string(plan->reports[1].data_set_ref, "IED1/AP1/LD0/LLN0.dsEvents", "unbuffered ReportControl DatSet ref");
        passed &= expect_true(plan->reports[1].data_set_index == 0U, "unbuffered ReportControl DataSet index");
        passed &= expect_true(plan->reports[1].conf_rev_known == 1, "unbuffered ConfRev should be known");
        passed &= expect_true(plan->reports[1].conf_rev == 8U, "unbuffered ConfRev value");

        passed &= expect_string(plan->signals[0].reference, "LD0/XCBR1.Pos.stVal[ST]", "first signal ref");
        passed &= expect_string(plan->signals[0].object_reference, "IED1LD0.XCBR1.Pos.stVal", "first signal object ref");
        passed &= expect_true(plan->signals[0].initial_value_kind == UNITLAB_IED_FIXTURE_VALUE_BOOLEAN, "first signal typed default kind");
        passed &= expect_string(plan->signals[0].initial_value, "false", "first signal typed default value");
        passed &= expect_string(plan->signals[1].reference, "LD0/XCBR1.Pos.origin.orIdent[ST]", "second signal nested ref");
        passed &= expect_string(plan->signals[1].object_reference, "IED1LD0.XCBR1.Pos.origin.orIdent", "second signal nested object ref");
        passed &= expect_true(plan->signals[1].initial_value_kind == UNITLAB_IED_FIXTURE_VALUE_STRING, "second signal nested typed default kind");
        passed &= expect_string(plan->signals[1].initial_value, "", "second signal nested typed default value");
        passed &= expect_string(plan->signals[2].reference, "LD0/XCBR1.Pos.origin.nested.deepIdent[ST]", "third signal deep nested ref");
        passed &= expect_string(plan->signals[2].object_reference, "IED1LD0.XCBR1.Pos.origin.nested.deepIdent", "third signal deep nested object ref");
        passed &= expect_true(plan->signals[2].initial_value_kind == UNITLAB_IED_FIXTURE_VALUE_STRING, "third signal deep nested typed default kind");
        passed &= expect_string(plan->signals[2].initial_value, "", "third signal deep nested typed default value");
        passed &= expect_string(plan->signals[3].reference, "LD0/XCBR1.Pos.ctlModel[CF]", "fourth signal enum ref");
        passed &= expect_string(plan->signals[3].object_reference, "IED1LD0.XCBR1.Pos.ctlModel", "fourth signal enum object ref");
        passed &= expect_true(plan->signals[3].initial_value_kind == UNITLAB_IED_FIXTURE_VALUE_INTEGER, "fourth signal enum default kind");
        passed &= expect_string(plan->signals[3].initial_value, "1", "fourth signal enum default value");
        passed &= expect_string(plan->signals[4].reference, "LD0/XCBR1.Beh.subState.stVal[ST]", "fifth signal SDO ref");
        passed &= expect_string(plan->signals[4].object_reference, "IED1LD0.XCBR1.Beh.subState.stVal", "fifth signal SDO object ref");
        passed &= expect_true(plan->signals[4].initial_value_kind == UNITLAB_IED_FIXTURE_VALUE_INTEGER, "fifth signal SDO typed default kind");
        passed &= expect_string(plan->signals[4].initial_value, "0", "fifth signal SDO typed default value");
        passed &= expect_string(plan->signals[5].reference, "LD0/PGGIO1.Ind1.stVal[ST]", "sixth signal FCD value ref");
        passed &= expect_string(plan->signals[5].object_reference, "IED1LD0.PGGIO1.Ind1.stVal", "sixth signal FCD value object ref");
        passed &= expect_string(plan->signals[5].data_attribute_path, "stVal", "sixth signal FCD value attribute");
        passed &= expect_string(plan->signals[5].data_set_entry_variable, "IED1LD0/PGGIO1$ST$Ind1$stVal", "sixth signal FCD value DataSet entry variable");
        passed &= expect_true(plan->signals[5].initial_value_kind == UNITLAB_IED_FIXTURE_VALUE_INTEGER, "sixth signal FCD value typed default kind");
        passed &= expect_string(plan->signals[5].initial_value, "0", "sixth signal FCD value typed default value");
        passed &= expect_string(plan->signals[6].reference, "LD0/PGGIO1.Ind1.q[ST]", "seventh signal FCD quality ref");
        passed &= expect_string(plan->signals[6].object_reference, "IED1LD0.PGGIO1.Ind1.q", "seventh signal FCD quality object ref");
        passed &= expect_string(plan->signals[6].data_attribute_path, "q", "seventh signal FCD quality attribute");
        passed &= expect_string(plan->signals[6].data_set_entry_variable, "IED1LD0/PGGIO1$ST$Ind1$q", "seventh signal FCD quality DataSet entry variable");
        passed &= expect_true(plan->signals[6].initial_value_kind == UNITLAB_IED_FIXTURE_VALUE_INTEGER, "seventh signal FCD quality typed default kind");
        passed &= expect_string(plan->signals[6].initial_value, "0", "seventh signal FCD quality typed default value");
        passed &= expect_string(plan->signals[7].reference, "LD0/PGGIO1.Ind1.t[ST]", "eighth signal FCD timestamp ref");
        passed &= expect_string(plan->signals[7].object_reference, "IED1LD0.PGGIO1.Ind1.t", "eighth signal FCD timestamp object ref");
        passed &= expect_string(plan->signals[7].data_attribute_path, "t", "eighth signal FCD timestamp attribute");
        passed &= expect_string(plan->signals[7].data_set_entry_variable, "IED1LD0/PGGIO1$ST$Ind1$t", "eighth signal FCD timestamp DataSet entry variable");
        passed &= expect_true(plan->signals[7].initial_value_kind == UNITLAB_IED_FIXTURE_VALUE_STRING, "eighth signal FCD timestamp typed default kind");
        passed &= expect_string(plan->signals[7].initial_value, "", "eighth signal FCD timestamp typed default value");
        passed &= expect_string(plan->signals[8].reference, "LD0/LedGGIO1.Ind1.stVal[ST]", "ninth signal FCDA DO-level value ref");
        passed &= expect_string(plan->signals[8].object_reference, "IED1LD0.LedGGIO1.Ind1.stVal", "ninth signal FCDA DO-level value object ref");
        passed &= expect_string(plan->signals[8].data_attribute_path, "stVal", "ninth signal FCDA DO-level value attribute");
        passed &= expect_string(plan->signals[8].data_set_entry_variable, "IED1LD0/LedGGIO1$ST$Ind1$stVal", "ninth signal FCDA DO-level value DataSet entry variable");
        passed &= expect_string(plan->signals[9].reference, "LD0/LedGGIO1.Ind1.q[ST]", "tenth signal FCDA DO-level quality ref");
        passed &= expect_string(plan->signals[9].data_set_entry_variable, "IED1LD0/LedGGIO1$ST$Ind1$q", "tenth signal FCDA DO-level quality DataSet entry variable");
        passed &= expect_string(plan->signals[10].reference, "LD0/LedGGIO1.Ind1.t[ST]", "eleventh signal FCDA DO-level timestamp ref");
        passed &= expect_string(plan->signals[10].data_set_entry_variable, "IED1LD0/LedGGIO1$ST$Ind1$t", "eleventh signal FCDA DO-level timestamp DataSet entry variable");
        passed &= expect_string(plan->signals[11].reference, "LD1/PGGIO1.Ind1.stVal[ST]", "twelfth signal cross-LD FCDA DO-level value ref");
        passed &= expect_string(plan->signals[11].object_reference, "IED1LD1.PGGIO1.Ind1.stVal", "twelfth signal cross-LD FCDA DO-level value object ref");
        passed &= expect_string(plan->signals[11].data_set_entry_variable, "IED1LD1/PGGIO1$ST$Ind1$stVal", "twelfth signal cross-LD FCDA DO-level value DataSet entry variable");
        passed &= expect_string(plan->signals[12].reference, "LD1/PGGIO1.Ind1.q[ST]", "thirteenth signal cross-LD quality ref");
        passed &= expect_string(plan->signals[13].reference, "LD1/PGGIO1.Ind1.t[ST]", "fourteenth signal cross-LD timestamp ref");
        passed &= expect_string(plan->signals[14].reference, "LD0/MMXU1.PhV.phsA.cVal.mag.f[MX]", "fifteenth signal CMV nested mag.f ref");
        passed &= expect_string(plan->signals[14].data_set_entry_variable, "IED1LD0/MMXU1$MX$PhV$phsA$cVal$mag$f", "fifteenth signal CMV canonical DataSet entry variable");
        passed &= expect_true(plan->signals[14].initial_value_kind == UNITLAB_IED_FIXTURE_VALUE_REAL, "fifteenth signal CMV mag.f typed default kind");
        passed &= expect_string(plan->signals[15].reference, "LD0/MMXU1.PhV.phsA.q[MX]", "sixteenth signal CMV quality ref");
        passed &= expect_string(plan->signals[16].reference, "LD0/MMXU1.PhV.phsA.t[MX]", "seventeenth signal CMV timestamp ref");
        passed &= expect_string(plan->signals[17].reference, "LD0/MMXU1.Hz.mag.f[MX]", "eighteenth signal MV mag.f ref");
        passed &= expect_string(plan->signals[17].data_set_entry_variable, "IED1LD0/MMXU1$MX$Hz$mag$f", "eighteenth signal MV canonical DataSet entry variable");
        passed &= expect_string(plan->signals[18].reference, "LD0/MMXU1.Hz.q[MX]", "nineteenth signal MV quality ref");
        passed &= expect_string(plan->signals[19].reference, "LD0/MMXU1.Hz.t[MX]", "twentieth signal MV timestamp ref");
    }

    size_t json_size = unitlab_scl_compile_normalized_json_size(result);
    char* json = NULL;
    size_t written_size = 0U;
    passed &= expect_true(json_size > 64U, "normalized JSON size should include payload and null terminator");
    passed &= expect_true(unitlab_scl_compile_normalized_json(result, error, 8U, &written_size) == 0,
        "normalized JSON should reject too-small buffer");
    passed &= expect_true(written_size == 0U, "failed normalized JSON write should not report bytes written");
    json = (char*)malloc(json_size);
    passed &= expect_true(json != NULL, "normalized JSON buffer should allocate");
    if (json != NULL) {
        passed &= expect_true(unitlab_scl_compile_normalized_json(result, json, json_size, &written_size) == 1,
            "normalized JSON should write into exact-size buffer");
        passed &= expect_true(written_size == json_size, "normalized JSON written size should include null terminator");
        passed &= expect_contains(json, "\"schema\":\"unitlab.iec61850.scl.normalized.v1\"", "normalized JSON schema");
        passed &= expect_contains(json, "\"selectedIed\":\"IED1\"", "normalized JSON selected IED");
        passed &= expect_contains(json, "\"logicalDevices\":[{\"inst\":\"IED1LD0\"},{\"inst\":\"IED1LD1\"}]", "normalized JSON logical devices");
        passed &= expect_contains(json, "\"reference\":\"LD0/PGGIO1.Ind1.q[ST]\"", "normalized JSON q signal");
        passed &= expect_contains(json, "\"dataAttributePath\":\"t\"", "normalized JSON t attribute");
        passed &= expect_contains(json, "\"reference\":\"LD0/LedGGIO1.Ind1.stVal[ST]\"", "normalized JSON FCDA DO-level prefixed signal");
        passed &= expect_contains(json, "\"diagnostics\":[]", "normalized JSON empty diagnostics");
        free(json);
    }

    unitlab_scl_compile_result_free(result);
    return passed;
}

static int test_compile_reports_invalid_dataset_member_and_missing_report_dataset(void)
{
    const char* scl =
        "<SCL><IED name=\"IED1\"><AccessPoint name=\"AP1\"><Server><LDevice inst=\"LD0\">"
        "<LN0>"
        "<DataSet name=\"dsBroken\">"
        "<FCDA ldInst=\"LD0\" lnClass=\"XCBR\" lnInst=\"1\" fc=\"ST\" />"
        "</DataSet>"
        "<ReportControl name=\"brcbBroken\" buffered=\"true\" datSet=\"missingDataSet\" />"
        "<ReportControl name=\"urcbNoDataSet\" buffered=\"false\" />"
        "</LN0>"
        "<LN lnClass=\"XCBR\" inst=\"1\" />"
        "</LDevice></Server></AccessPoint></IED></SCL>";
    UnitLabSclCompileResult* result = NULL;
    char error[128];
    int passed = 1;

    passed &= expect_true(unitlab_scl_compile_from_memory(scl, strlen(scl), "IED1", &result, error, sizeof(error)) == 1,
        "broken SCL should return structured diagnostics through C API");
    passed &= expect_true(result != NULL, "broken SCL result should be allocated");
    passed &= expect_true(unitlab_scl_compile_diagnostic_count(result) == 3U, "invalid member, missing report DataSet, and empty report DataSet diagnostics");

    UnitLabSclCompileDiagnostic diagnostic;
    passed &= expect_true(unitlab_scl_compile_diagnostic_at(result, 0U, &diagnostic) == 1, "invalid member diagnostic should be readable");
    passed &= expect_string(diagnostic.severity, "error", "invalid member severity");
    passed &= expect_string(diagnostic.code, "SCL_DATASET_MEMBER_INVALID", "invalid member code");
    passed &= expect_string(diagnostic.ied_name, "IED1", "invalid member IED context");
    passed &= expect_string(diagnostic.access_point_name, "AP1", "invalid member AccessPoint context");
    passed &= expect_string(diagnostic.logical_device_inst, "LD0", "invalid member LD context");
    passed &= expect_string(diagnostic.logical_node_name, "LLN0", "invalid member LN context");
    passed &= expect_string(diagnostic.data_set_name, "dsBroken", "invalid member DataSet context");
    passed &= expect_string(diagnostic.member_reference, "LD0/XCBR1[ST]", "invalid member reference context");
    passed &= expect_true(unitlab_scl_compile_diagnostic_at(result, 1U, &diagnostic) == 1, "missing DataSet diagnostic should be readable");
    passed &= expect_string(diagnostic.severity, "error", "missing DataSet severity");
    passed &= expect_string(diagnostic.code, "SCL_REPORT_DATASET_MISSING", "missing DataSet code");
    passed &= expect_string(diagnostic.ied_name, "IED1", "missing DataSet IED context");
    passed &= expect_string(diagnostic.access_point_name, "AP1", "missing DataSet AccessPoint context");
    passed &= expect_string(diagnostic.logical_device_inst, "LD0", "missing DataSet LD context");
    passed &= expect_string(diagnostic.logical_node_name, "LLN0", "missing DataSet LN context");
    passed &= expect_string(diagnostic.data_set_name, "missingDataSet", "missing DataSet context");
    passed &= expect_string(diagnostic.report_control_name, "brcbBroken", "missing DataSet ReportControl context");
    passed &= expect_true(unitlab_scl_compile_diagnostic_at(result, 2U, &diagnostic) == 1, "empty DataSet diagnostic should be readable");
    passed &= expect_string(diagnostic.severity, "warning", "empty DataSet severity");
    passed &= expect_string(diagnostic.code, "SCL_REPORT_DATASET_EMPTY", "empty DataSet code");
    passed &= expect_string(diagnostic.data_set_name, "", "empty DataSet context");
    passed &= expect_string(diagnostic.report_control_name, "urcbNoDataSet", "empty DataSet ReportControl context");

    const UnitLabIedModelPlan* plan = unitlab_scl_compile_model_plan(result);
    passed &= expect_true(plan != NULL, "partial model plan should be readable");
    if (plan != NULL) {
        passed &= expect_true(plan->data_set_count == 1U, "broken DataSet should still be represented");
        passed &= expect_true(plan->data_sets[0].member_count == 0U, "invalid DataSet member should not become a signal");
        passed &= expect_true(plan->signal_count == 0U, "invalid member should not produce runtime signal");
        passed &= expect_true(plan->report_count == 0U, "ReportControl with missing DataSet should not compile");
    }

    unitlab_scl_compile_result_free(result);
    return passed;
}


static int test_compile_reports_unresolved_sdo_path(void)
{
    const char* scl =
        "<SCL><IED name=\"IED1\"><AccessPoint name=\"AP1\"><Server><LDevice inst=\"LD0\">"
        "<LN0><DataSet name=\"dsBroken\">"
        "<FCDA ldInst=\"LD0\" lnClass=\"XCBR\" lnInst=\"1\" doName=\"Beh.subState\" daName=\"stVal\" fc=\"ST\" />"
        "</DataSet></LN0>"
        "<LN lnClass=\"XCBR\" inst=\"1\" lnType=\"XCBR_TYPE\" />"
        "</LDevice></Server></AccessPoint></IED>"
        "<DataTypeTemplates>"
        "<LNodeType id=\"XCBR_TYPE\" lnClass=\"XCBR\"><DO name=\"Beh\" type=\"BEH_ROOT\" /></LNodeType>"
        "<DOType id=\"BEH_ROOT\" cdc=\"ENS\" />"
        "</DataTypeTemplates></SCL>";
    UnitLabSclCompileResult* result = NULL;
    char error[128];
    int passed = 1;

    passed &= expect_true(unitlab_scl_compile_from_memory(scl, strlen(scl), "IED1", &result, error, sizeof(error)) == 1,
        "unresolved SDO should return structured diagnostics through C API");
    passed &= expect_true(result != NULL, "unresolved SDO result should be allocated");
    passed &= expect_true(unitlab_scl_compile_diagnostic_count(result) == 1U, "unresolved SDO diagnostic should be present");

    UnitLabSclCompileDiagnostic diagnostic;
    passed &= expect_true(unitlab_scl_compile_diagnostic_at(result, 0U, &diagnostic) == 1, "unresolved SDO diagnostic should be readable");
    passed &= expect_string(diagnostic.severity, "error", "unresolved SDO severity");
    passed &= expect_string(diagnostic.code, "SCL_TEMPLATE_SDO_MISSING", "unresolved SDO code");
    passed &= expect_string(diagnostic.member_reference, "LD0/XCBR1.Beh.subState.stVal[ST]", "unresolved SDO member reference context");

    const UnitLabIedModelPlan* plan = unitlab_scl_compile_model_plan(result);
    passed &= expect_true(plan != NULL, "partial model plan should be readable for unresolved SDO");
    if (plan != NULL) {
        passed &= expect_true(plan->data_set_count == 1U, "unresolved SDO DataSet should still be represented");
        passed &= expect_true(plan->data_sets[0].member_count == 0U, "unresolved SDO member should not become a DataSet signal");
        passed &= expect_true(plan->signal_count == 0U, "unresolved SDO should not produce runtime signal");
    }

    unitlab_scl_compile_result_free(result);
    return passed;
}


static int test_compile_reports_unresolved_nested_attribute_path(void)
{
    const char* scl =
        "<SCL><IED name=\"IED1\"><AccessPoint name=\"AP1\"><Server><LDevice inst=\"LD0\">"
        "<LN0><DataSet name=\"dsBroken\">"
        "<FCDA ldInst=\"LD0\" lnClass=\"XCBR\" lnInst=\"1\" doName=\"Pos\" daName=\"origin.missing.deepIdent\" fc=\"ST\" />"
        "</DataSet></LN0>"
        "<LN lnClass=\"XCBR\" inst=\"1\" lnType=\"XCBR_TYPE\" />"
        "</LDevice></Server></AccessPoint></IED>"
        "<DataTypeTemplates>"
        "<LNodeType id=\"XCBR_TYPE\" lnClass=\"XCBR\"><DO name=\"Pos\" type=\"DPC_POS\" /></LNodeType>"
        "<DOType id=\"DPC_POS\" cdc=\"DPC\"><DA name=\"origin\" fc=\"ST\" bType=\"Struct\" type=\"ORIGINATOR\" /></DOType>"
        "<DAType id=\"ORIGINATOR\"><BDA name=\"orIdent\" bType=\"VisString64\" /></DAType>"
        "</DataTypeTemplates></SCL>";
    UnitLabSclCompileResult* result = NULL;
    char error[128];
    int passed = 1;

    passed &= expect_true(unitlab_scl_compile_from_memory(scl, strlen(scl), "IED1", &result, error, sizeof(error)) == 1,
        "unresolved nested attribute should return structured diagnostics through C API");
    passed &= expect_true(result != NULL, "unresolved nested attribute result should be allocated");
    passed &= expect_true(unitlab_scl_compile_diagnostic_count(result) == 1U, "unresolved nested attribute diagnostic should be present");

    UnitLabSclCompileDiagnostic diagnostic;
    passed &= expect_true(unitlab_scl_compile_diagnostic_at(result, 0U, &diagnostic) == 1, "unresolved nested attribute diagnostic should be readable");
    passed &= expect_string(diagnostic.severity, "error", "unresolved nested attribute severity");
    passed &= expect_string(diagnostic.code, "SCL_TEMPLATE_BDA_MISSING", "unresolved nested attribute code");
    passed &= expect_string(diagnostic.member_reference, "LD0/XCBR1.Pos.origin.missing.deepIdent[ST]", "unresolved nested attribute member reference context");

    const UnitLabIedModelPlan* plan = unitlab_scl_compile_model_plan(result);
    passed &= expect_true(plan != NULL, "partial model plan should be readable for unresolved nested attribute");
    if (plan != NULL) {
        passed &= expect_true(plan->data_set_count == 1U, "unresolved nested attribute DataSet should still be represented");
        passed &= expect_true(plan->data_sets[0].member_count == 0U, "unresolved nested attribute member should not become a DataSet signal");
        passed &= expect_true(plan->signal_count == 0U, "unresolved nested attribute should not produce runtime signal");
    }

    unitlab_scl_compile_result_free(result);
    return passed;
}


static int test_compile_reports_missing_template_kinds(void)
{
    const char* scl =
        "<SCL><IED name=\"IED1\"><AccessPoint name=\"AP1\"><Server><LDevice inst=\"LD0\">"
        "<LN0><DataSet name=\"dsBroken\">"
        "<FCDA ldInst=\"LD0\" lnClass=\"XCBR\" lnInst=\"1\" doName=\"Pos\" daName=\"stVal\" fc=\"ST\" />"
        "<FCDA ldInst=\"LD0\" lnClass=\"XCBR\" lnInst=\"2\" doName=\"Pos\" daName=\"stVal\" fc=\"ST\" />"
        "<FCDA ldInst=\"LD0\" lnClass=\"XCBR\" lnInst=\"3\" doName=\"Pos\" daName=\"stVal\" fc=\"ST\" />"
        "<FCDA ldInst=\"LD0\" lnClass=\"XCBR\" lnInst=\"4\" doName=\"Pos\" daName=\"stVal\" fc=\"ST\" />"
        "<FCDA ldInst=\"LD0\" lnClass=\"XCBR\" lnInst=\"5\" doName=\"Pos\" daName=\"origin.orIdent\" fc=\"ST\" />"
        "<FCDA ldInst=\"LD0\" lnClass=\"XCBR\" lnInst=\"6\" doName=\"Pos\" daName=\"ctlModel\" fc=\"CF\" />"
        "</DataSet></LN0>"
        "<LN lnClass=\"XCBR\" inst=\"1\" lnType=\"MISSING_LNODE_TYPE\" />"
        "<LN lnClass=\"XCBR\" inst=\"2\" lnType=\"LNODE_MISSING_DO\" />"
        "<LN lnClass=\"XCBR\" inst=\"3\" lnType=\"LNODE_MISSING_DOTYPE\" />"
        "<LN lnClass=\"XCBR\" inst=\"4\" lnType=\"LNODE_MISSING_DA\" />"
        "<LN lnClass=\"XCBR\" inst=\"5\" lnType=\"LNODE_MISSING_DATYPE\" />"
        "<LN lnClass=\"XCBR\" inst=\"6\" lnType=\"LNODE_MISSING_ENUM\" />"
        "</LDevice></Server></AccessPoint></IED>"
        "<DataTypeTemplates>"
        "<LNodeType id=\"LNODE_MISSING_DO\" lnClass=\"XCBR\" />"
        "<LNodeType id=\"LNODE_MISSING_DOTYPE\" lnClass=\"XCBR\"><DO name=\"Pos\" type=\"MISSING_DO_TYPE\" /></LNodeType>"
        "<LNodeType id=\"LNODE_MISSING_DA\" lnClass=\"XCBR\"><DO name=\"Pos\" type=\"DPC_EMPTY\" /></LNodeType>"
        "<LNodeType id=\"LNODE_MISSING_DATYPE\" lnClass=\"XCBR\"><DO name=\"Pos\" type=\"DPC_MISSING_DATYPE\" /></LNodeType>"
        "<LNodeType id=\"LNODE_MISSING_ENUM\" lnClass=\"XCBR\"><DO name=\"Pos\" type=\"DPC_MISSING_ENUM\" /></LNodeType>"
        "<DOType id=\"DPC_EMPTY\" cdc=\"DPC\" />"
        "<DOType id=\"DPC_MISSING_DATYPE\" cdc=\"DPC\"><DA name=\"origin\" fc=\"ST\" bType=\"Struct\" type=\"MISSING_DA_TYPE\" /></DOType>"
        "<DOType id=\"DPC_MISSING_ENUM\" cdc=\"DPC\"><DA name=\"ctlModel\" fc=\"CF\" bType=\"Enum\" type=\"MISSING_ENUM\" /></DOType>"
        "</DataTypeTemplates></SCL>";
    UnitLabSclCompileResult* result = NULL;
    char error[128];
    int passed = 1;

    passed &= expect_true(unitlab_scl_compile_from_memory(scl, strlen(scl), "IED1", &result, error, sizeof(error)) == 1,
        "missing templates should return structured diagnostics through C API");
    passed &= expect_true(result != NULL, "missing template result should be allocated");
    passed &= expect_true(unitlab_scl_compile_diagnostic_count(result) == 6U, "missing template diagnostics should be present");

    UnitLabSclCompileDiagnostic diagnostic;
    passed &= expect_true(unitlab_scl_compile_diagnostic_at(result, 0U, &diagnostic) == 1, "missing LNodeType diagnostic should be readable");
    passed &= expect_string(diagnostic.code, "SCL_TEMPLATE_LNODETYPE_MISSING", "missing LNodeType code");
    passed &= expect_string(diagnostic.member_reference, "LD0/XCBR1.Pos.stVal[ST]", "missing LNodeType member ref");
    passed &= expect_true(unitlab_scl_compile_diagnostic_at(result, 1U, &diagnostic) == 1, "missing DO diagnostic should be readable");
    passed &= expect_string(diagnostic.code, "SCL_TEMPLATE_DO_MISSING", "missing DO code");
    passed &= expect_true(unitlab_scl_compile_diagnostic_at(result, 2U, &diagnostic) == 1, "missing DOType diagnostic should be readable");
    passed &= expect_string(diagnostic.code, "SCL_TEMPLATE_DOTYPE_MISSING", "missing DOType code");
    passed &= expect_true(unitlab_scl_compile_diagnostic_at(result, 3U, &diagnostic) == 1, "missing DA diagnostic should be readable");
    passed &= expect_string(diagnostic.code, "SCL_TEMPLATE_DA_MISSING", "missing DA code");
    passed &= expect_true(unitlab_scl_compile_diagnostic_at(result, 4U, &diagnostic) == 1, "missing DAType diagnostic should be readable");
    passed &= expect_string(diagnostic.code, "SCL_TEMPLATE_DATYPE_MISSING", "missing DAType code");
    passed &= expect_true(unitlab_scl_compile_diagnostic_at(result, 5U, &diagnostic) == 1, "missing EnumType diagnostic should be readable");
    passed &= expect_string(diagnostic.code, "SCL_TEMPLATE_ENUMTYPE_MISSING", "missing EnumType code");

    const UnitLabIedModelPlan* plan = unitlab_scl_compile_model_plan(result);
    passed &= expect_true(plan != NULL, "partial model plan should be readable for missing templates");
    if (plan != NULL) {
        passed &= expect_true(plan->data_set_count == 1U, "missing template DataSet should still be represented");
        passed &= expect_true(plan->data_sets[0].member_count == 0U, "missing template members should not become DataSet signals");
        passed &= expect_true(plan->signal_count == 0U, "missing template members should not produce runtime signals");
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
    passed &= expect_string(diagnostic.ied_name, "MISSING", "missing IED context");

    unitlab_scl_compile_result_free(result);
    return passed;
}

static int test_compile_reports_malformed_xml(void)
{
    const char* scl = "<SCL><IED name=\"IED1\"></SCL>";
    UnitLabSclCompileResult* result = NULL;
    char error[128];
    int passed = 1;

    passed &= expect_true(unitlab_scl_compile_from_memory(scl, strlen(scl), "IED1", &result, error, sizeof(error)) == 1,
        "malformed XML should return structured diagnostics");
    passed &= expect_true(result != NULL, "malformed XML result should be allocated");
    passed &= expect_true(unitlab_scl_compile_diagnostic_count(result) == 1U, "malformed XML diagnostic should be present");

    UnitLabSclCompileDiagnostic diagnostic;
    passed &= expect_true(unitlab_scl_compile_diagnostic_at(result, 0U, &diagnostic) == 1, "malformed XML diagnostic should be readable");
    passed &= expect_string(diagnostic.severity, "error", "malformed XML severity");
    passed &= expect_string(diagnostic.code, "SCL_XML_PARSE_FAILED", "malformed XML code");
    passed &= expect_true(unitlab_scl_compile_model_plan(result) != NULL, "malformed XML empty plan should be readable");
    passed &= expect_true(unitlab_scl_compile_model_plan(result)->logical_device_count == 0U, "malformed XML should not compile model");
    size_t json_size = unitlab_scl_compile_normalized_json_size(result);
    char* json = (char*)malloc(json_size);
    size_t written_size = 0U;
    passed &= expect_true(json != NULL, "malformed XML normalized JSON buffer should allocate");
    if (json != NULL) {
        passed &= expect_true(unitlab_scl_compile_normalized_json(result, json, json_size, &written_size) == 1,
            "malformed XML normalized JSON should write diagnostics");
        passed &= expect_contains(json, "\"code\":\"SCL_XML_PARSE_FAILED\"", "malformed XML normalized JSON diagnostic code");
        free(json);
    }

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
    passed &= test_compile_reports_invalid_dataset_member_and_missing_report_dataset();
    passed &= test_compile_reports_unresolved_sdo_path();
    passed &= test_compile_reports_unresolved_nested_attribute_path();
    passed &= test_compile_reports_missing_template_kinds();
    passed &= test_compile_reports_invalid_selected_ied();
    passed &= test_compile_reports_malformed_xml();
    passed &= test_compile_rejects_empty_input();
    return passed ? 0 : 1;
}
