#include "scl_compiler/unitlab_scl_compiler.h"
#include "server/unitlab_mms_server_runtime.h"
#include "server/unitlab_mms_server_runtime_internal.h"
#include "model/model_plan.h"
#include "wire/acse/unitlab_mms_acse.h"
#include "wire/mms/unitlab_mms_pdu.h"

#include <assert.h>
#include <string.h>

static const char* smoke_scl(void)
{
    return
        "<?xml version=\"1.0\"?>"
        "<SCL>"
        "<IED name=\"IED1\"><AccessPoint name=\"AP1\"><Server><LDevice inst=\"LD0\">"
        "<LN0 lnType=\"LLN0_TYPE\">"
        "<DataSet name=\"dsEvents\">"
        "<FCDA ldInst=\"LD0\" lnClass=\"XCBR\" lnInst=\"1\" doName=\"Pos\" daName=\"stVal\" fc=\"ST\" />"
        "<FCD ldInst=\"LD0\" lnClass=\"PGGIO\" lnInst=\"1\" doName=\"Ind1\" fc=\"ST\" />"
        "</DataSet>"
        "<ReportControl name=\"brcbEvents\" buffered=\"true\" rptID=\"events\" datSet=\"dsEvents\" confRev=\"7\" indexed=\"false\" bufTime=\"100\" intgPd=\"1000\">"
        "<TrgOps dchg=\"true\" qchg=\"true\" dupd=\"false\" period=\"false\" gi=\"true\" />"
        "<OptFields seqNum=\"true\" timeStamp=\"true\" reasonCode=\"true\" dataSet=\"true\" dataRef=\"true\" bufOvfl=\"true\" entryID=\"true\" configRef=\"true\" />"
        "</ReportControl>"
        "</LN0>"
        "<LN lnClass=\"XCBR\" inst=\"1\" lnType=\"XCBR_TYPE\" />"
        "<LN lnClass=\"PGGIO\" inst=\"1\" lnType=\"PGGIO_TYPE\" />"
        "</LDevice></Server></AccessPoint></IED>"
        "<DataTypeTemplates>"
        "<LNodeType id=\"XCBR_TYPE\" lnClass=\"XCBR\"><DO name=\"Pos\" type=\"DPC_POS\" /></LNodeType>"
        "<LNodeType id=\"PGGIO_TYPE\" lnClass=\"PGGIO\"><DO name=\"Ind1\" type=\"INS_IND\" /></LNodeType>"
        "<DOType id=\"DPC_POS\" cdc=\"DPC\"><DA name=\"stVal\" fc=\"ST\" bType=\"BOOLEAN\" /></DOType>"
        "<DOType id=\"INS_IND\" cdc=\"INS\"><DA name=\"stVal\" fc=\"ST\" bType=\"INT32\" /><DA name=\"q\" fc=\"ST\" bType=\"Quality\" /><DA name=\"t\" fc=\"ST\" bType=\"Timestamp\" /></DOType>"
        "</DataTypeTemplates>"
        "</SCL>";
}

static int name_list_contains(char** names, size_t count, const char* expected)
{
    for (size_t index = 0U; index < count; index++) {
        if (strcmp(names[index], expected) == 0) return 1;
    }
    return 0;
}

static int contains_bytes(const uint8_t* haystack, size_t haystack_length, const uint8_t* needle, size_t needle_length)
{
    if (haystack == NULL || needle == NULL || needle_length == 0U || haystack_length < needle_length) return 0;
    for (size_t index = 0U; index + needle_length <= haystack_length; index++) {
        if (memcmp(&haystack[index], needle, needle_length) == 0) return 1;
    }
    return 0;
}

int main(void)
{
    UnitLabSclCompileResult* compile_result = NULL;
    const UnitLabIedModelPlan* plan = NULL;
    UnitLabMmsServerRuntime runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabIedServerConfig config = { .bind_address = "127.0.0.1", .port = 102 };
    char error[256U];
    char** names = NULL;
    size_t name_count = 0U;
    const UnitLabIedModelDataSet* data_set = NULL;
    const UnitLabIedModelReportControl* report = NULL;
    const UnitLabIedModelSignal* signal = NULL;
    uint8_t value_bytes[32U];
    size_t value_length = 0U;
    char rpt_id_reference[256U];
    char data_set_reference[256U];
    size_t json_size = 0U;
    uint8_t report_bytes[4096U];
    uint8_t data_change_report_bytes[4096U];
    size_t report_length = 0U;
    size_t data_change_report_length = 0U;

    const char* scl = smoke_scl();
    assert(unitlab_scl_compile_from_memory(scl, strlen(scl), "IED1", &compile_result, error, sizeof(error)) == 1);
    assert(compile_result != NULL);
    assert(unitlab_scl_compile_diagnostic_count(compile_result) == 0U);
    plan = unitlab_scl_compile_model_plan(compile_result);
    assert(plan != NULL);
    assert(plan->logical_device_count == 1U);
    assert(plan->logical_node_count == 3U);
    assert(plan->data_set_count == 1U);
    assert(plan->report_count == 1U);
    assert(plan->signal_count == 4U);

    json_size = unitlab_scl_compile_normalized_json_size(compile_result);
    assert(json_size > 64U);

    unitlab_mms_server_runtime_init(&runtime);
    assert(unitlab_mms_server_runtime_apply_model_plan(&runtime, plan) == 1);
    assert(unitlab_mms_server_runtime_prepare(&runtime, &config, &diagnostic) == 1);
    assert(unitlab_mms_server_runtime_start(&runtime, &diagnostic) == 1);

    assert(unitlab_collect_ied_model_logical_devices(plan, &names, &name_count, error, sizeof(error)) == 1);
    assert(name_count == 1U);
    assert(name_list_contains(names, name_count, "IED1LD0") == 1);
    unitlab_free_ied_model_name_list(names, name_count);
    names = NULL;
    name_count = 0U;

    assert(unitlab_collect_ied_model_logical_node_data_sets(plan, "IED1LD0", "LLN0", &names, &name_count, error, sizeof(error)) == 1);
    assert(name_count == 1U);
    assert(name_list_contains(names, name_count, "dsEvents") == 1);
    unitlab_free_ied_model_name_list(names, name_count);
    names = NULL;
    name_count = 0U;

    data_set = unitlab_find_ied_model_data_set(plan, "IED1LD0", "LLN0", "dsEvents");
    assert(data_set != NULL);
    assert(data_set->member_count == 4U);
    signal = server_runtime_data_set_member_signal(&runtime, data_set, 0U);
    assert(signal != NULL);
    assert(strcmp(signal->object_reference, "IED1LD0.XCBR1.Pos.stVal") == 0);

    signal = server_runtime_find_signal_by_object_reference(&runtime, "IED1LD0.PGGIO1.Ind1.stVal");
    assert(signal != NULL);
    assert(strcmp(signal->data_attribute_path, "stVal") == 0);
    assert(unitlab_mms_server_runtime_update_signal_int32(&runtime, "IED1LD0.PGGIO1.Ind1.stVal", 42, &diagnostic) == 1);
    assert(server_runtime_encode_current_signal_value(&runtime, signal, value_bytes, sizeof(value_bytes), &value_length, &diagnostic) == 1);
    assert(value_length >= 2U);
    assert(value_bytes[value_length - 1U] == 42U);

    assert(server_runtime_find_signal_by_object_reference(&runtime, "IED1LD0.PGGIO1.Ind1.q") != NULL);
    assert(server_runtime_find_signal_by_object_reference(&runtime, "IED1LD0.PGGIO1.Ind1.t") != NULL);

    report = unitlab_find_ied_model_report_control(plan, "IED1LD0", "LLN0", "brcbEvents");
    assert(report != NULL);
    assert(report->is_buffered == 1);
    assert(report->data_set_index == 0U);
    assert(report->conf_rev_known == 1);
    assert(report->conf_rev == 7U);
    assert((report->trigger_options_mask & UNITLAB_IED_MODEL_TRG_OPT_DATA_CHANGED) != 0U);
    assert((report->optional_fields_mask & UNITLAB_IED_MODEL_RPT_OPT_DATA_REFERENCE) != 0U);
    server_runtime_format_report_control_references(&runtime, rpt_id_reference, sizeof(rpt_id_reference), data_set_reference, sizeof(data_set_reference));
    assert(strcmp(rpt_id_reference, "events") == 0);
    assert(strcmp(data_set_reference, "IED1LD0/LLN0$dsEvents") == 0);

    assert(unitlab_mms_server_runtime_reserve_report_control(&runtime, &diagnostic) == 1);
    assert(unitlab_mms_server_runtime_enable_report_control(&runtime, &diagnostic) == 1);
    runtime.brcb_rpt_ena = 1U;
    assert(unitlab_mms_server_runtime_request_general_interrogation(&runtime, &diagnostic) == 1);
    assert(unitlab_mms_server_runtime_build_pending_gi_report_bytes(&runtime, report_bytes, sizeof(report_bytes), &report_length, &diagnostic) == 1);
    assert(unitlab_mms_server_runtime_has_pending_gi_report(&runtime) == 0);
    assert(report_length > 0U);
    assert(contains_bytes(report_bytes, report_length, (const uint8_t*)"IED1LD0/XCBR1$ST$Pos$stVal", strlen("IED1LD0/XCBR1$ST$Pos$stVal")) == 1);
    assert(contains_bytes(report_bytes, report_length, (const uint8_t*)"IED1LD0/PGGIO1$ST$Ind1$stVal", strlen("IED1LD0/PGGIO1$ST$Ind1$stVal")) == 1);
    assert(contains_bytes(report_bytes, report_length, (const uint8_t*)"IED1LD0/PGGIO1$ST$Ind1$q", strlen("IED1LD0/PGGIO1$ST$Ind1$q")) == 1);
    assert(contains_bytes(report_bytes, report_length, (const uint8_t*)"IED1LD0/PGGIO1$ST$Ind1$t", strlen("IED1LD0/PGGIO1$ST$Ind1$t")) == 1);
    assert(contains_bytes(report_bytes, report_length, (const uint8_t*)"IED1LD0/LLN0$dsEvents", strlen("IED1LD0/LLN0$dsEvents")) == 1);
    assert(contains_bytes(report_bytes, report_length, (const uint8_t*)"IED1IED1LD0", strlen("IED1IED1LD0")) == 0);
    assert((report->optional_fields_mask & UNITLAB_IED_MODEL_RPT_OPT_REASON_FOR_INCLUSION) != 0U);
    assert(runtime.brcb_sq_num == 1U);

    assert(unitlab_mms_server_runtime_update_signal_int32(&runtime, "IED1LD0/PGGIO1$ST$Ind1$stVal", 43, &diagnostic) == 1);
    assert(runtime.pending_report_kind == UNITLAB_MMS_SERVER_PENDING_REPORT_DATA_CHANGE);
    assert(runtime.pending_report_member_index == 1U);
    assert((runtime.pending_report_member_mask & 0x02U) == 0x02U);
    assert(unitlab_mms_server_runtime_build_pending_gi_report_bytes(&runtime, data_change_report_bytes, sizeof(data_change_report_bytes), &data_change_report_length, &diagnostic) == 1);
    assert(data_change_report_length > 0U);
    assert(contains_bytes(data_change_report_bytes, data_change_report_length, (const uint8_t*)"IED1LD0/PGGIO1$ST$Ind1$stVal", strlen("IED1LD0/PGGIO1$ST$Ind1$stVal")) == 1);
    assert(contains_bytes(data_change_report_bytes, data_change_report_length, (const uint8_t*)"IED1LD0/PGGIO1$ST$Ind1$q", strlen("IED1LD0/PGGIO1$ST$Ind1$q")) == 1);
    assert(contains_bytes(data_change_report_bytes, data_change_report_length, (const uint8_t*)"IED1LD0/PGGIO1$ST$Ind1$t", strlen("IED1LD0/PGGIO1$ST$Ind1$t")) == 1);
    assert(contains_bytes(data_change_report_bytes, data_change_report_length, (const uint8_t*)"IED1LD0/XCBR1$ST$Pos$stVal", strlen("IED1LD0/XCBR1$ST$Pos$stVal")) == 0);
    assert(contains_bytes(data_change_report_bytes, data_change_report_length, (const uint8_t*)"\x84\x02\x04\x70", 4U) == 1);
    assert(runtime.pending_report_kind == UNITLAB_MMS_SERVER_PENDING_REPORT_NONE);

    unitlab_scl_compile_result_free(compile_result);
    return 0;
}
