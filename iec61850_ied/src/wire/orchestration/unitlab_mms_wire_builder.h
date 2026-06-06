#ifndef UNITLAB_IEC61850_IED_UNITLAB_MMS_WIRE_BUILDER_H
#define UNITLAB_IEC61850_IED_UNITLAB_MMS_WIRE_BUILDER_H

#include "wire/mms/unitlab_mms_pdu.h"
#include "model/model_plan.h"


#define UNITLAB_MMS_INITIATE_RESPONSE_PROFILE_PARAMETER_CBB_LENGTH 3U
#define UNITLAB_MMS_INITIATE_RESPONSE_PROFILE_SERVICES_SUPPORTED_LENGTH 12U

typedef struct UnitLabMmsInitiateResponseProfile {
    uint32_t local_detail_called;
    uint32_t max_serv_outstanding_calling;
    uint32_t max_serv_outstanding_called;
    uint32_t data_structure_nesting_level;
    uint8_t negotiated_version_number;
    uint8_t parameter_cbb[UNITLAB_MMS_INITIATE_RESPONSE_PROFILE_PARAMETER_CBB_LENGTH];
    size_t parameter_cbb_length;
    uint8_t services_supported_called[UNITLAB_MMS_INITIATE_RESPONSE_PROFILE_SERVICES_SUPPORTED_LENGTH];
    size_t services_supported_called_length;
} UnitLabMmsInitiateResponseProfile;

void unitlab_mms_initiate_response_profile_init(UnitLabMmsInitiateResponseProfile* profile);
void unitlab_mms_initiate_response_profile_apply_model_plan(UnitLabMmsInitiateResponseProfile* profile, const UnitLabIedModelPlan* plan);

/* Builds a wire frame by owning the nesting construction.
 * The caller provides a semantic MMS PDU, and this helper wraps it through the
 * association frame into TPKT + COTP + X.225 session + exact X.226 Presentation.
 */
int unitlab_mms_build_wire_frame_from_pdu(
    const UnitLabMmsPdu* pdu,
    uint8_t* scratch,
    size_t scratch_length,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic);

int unitlab_mms_build_confirmed_response_frame(
    const UnitLabMmsPdu* response_pdu,
    uint8_t* scratch,
    size_t scratch_length,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic);

int unitlab_mms_build_cotp_connect_request_frame(
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic);

int unitlab_mms_build_cotp_connect_response_frame(
    const uint8_t* request_parameters,
    size_t request_parameters_length,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic);

int unitlab_mms_build_association_response_frame_with_profile(
    const UnitLabMmsInitiateResponseProfile* profile,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic);

int unitlab_mms_build_association_response_frame(
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic);

int unitlab_mms_build_read_request_frame(
    const char* domain_id,
    const char* item_id,
    uint32_t invoke_id,
    uint8_t* scratch,
    size_t scratch_length,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic);

int unitlab_mms_build_write_request_frame(
    const char* domain_id,
    const char* item_id,
    const UnitLabMmsBerElement* data_element,
    uint32_t invoke_id,
    uint8_t* scratch,
    size_t scratch_length,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic);

int unitlab_mms_build_get_variable_access_attributes_request_frame(
    const char* domain_id,
    const char* item_id,
    uint32_t invoke_id,
    uint8_t* scratch,
    size_t scratch_length,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic);

int unitlab_mms_build_get_name_list_request_frame(
    uint32_t object_class,
    uint32_t object_scope,
    const char* domain_id,
    const char* continue_after,
    uint32_t invoke_id,
    uint8_t* scratch,
    size_t scratch_length,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic);

int unitlab_mms_build_information_report_frame(
    const char* item_id,
    uint8_t boolean_value,
    uint8_t* scratch,
    size_t scratch_length,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic);

#endif /* UNITLAB_IEC61850_IED_UNITLAB_MMS_WIRE_BUILDER_H */
