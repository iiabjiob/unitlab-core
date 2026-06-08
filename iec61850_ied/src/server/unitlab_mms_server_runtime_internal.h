#ifndef UNITLAB_IEC61850_IED_UNITLAB_MMS_SERVER_RUNTIME_INTERNAL_H
#define UNITLAB_IEC61850_IED_UNITLAB_MMS_SERVER_RUNTIME_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

#include "server/unitlab_mms_server_runtime.h"
#include "model/model_plan.h"
#include "wire/ber/unitlab_mms_ber.h"

/* Internal service boundary: runtime orchestrates, service modules build MMS payloads. */

void server_runtime_set_diagnostic(UnitLabMmsDiagnostic* diagnostic, UnitLabMmsDiagnosticCode code, const char* message);
const char* server_runtime_service_name_for_kind(UnitLabMmsServiceKind service_kind);
const char* server_runtime_decoded_service_name_for_pdu(const UnitLabMmsPdu* wire_pdu);
int server_runtime_tag_to_hex(const UnitLabMmsBerTag* tag, char* buffer, size_t buffer_length);
int server_runtime_bytes_are_printable_ascii(const uint8_t* bytes, size_t length);
void server_runtime_print_hex_bytes(const uint8_t* bytes, size_t length);
void server_runtime_store_incoming_context(UnitLabMmsServerRuntime* server_runtime, uint32_t invoke_id, const char* service_name);
void server_runtime_store_outgoing_context(UnitLabMmsServerRuntime* server_runtime, uint32_t invoke_id, const char* service_name, const char* summary);
void server_runtime_store_read_summary(UnitLabMmsServerRuntime* server_runtime, uint32_t invoke_id, int value_supported, const uint8_t* value_bytes, size_t value_length);
void server_runtime_store_name_list_summary(UnitLabMmsServerRuntime* server_runtime, uint32_t invoke_id, const char* service_name, const char* label, const char* const* names, size_t name_count);
void server_runtime_log_confirmed_response_preview(UnitLabMmsServerRuntime* server_runtime, uint32_t invoke_id, const char* service_name, const uint8_t* service_bytes, size_t service_length);
int server_runtime_validate_confirmed_response_payload(const char* service_name, const uint8_t* service_bytes, size_t service_length, UnitLabMmsDiagnostic* diagnostic);

int server_runtime_encode_ber_element(UnitLabMmsBerTagClass tag_class, int constructed, uint32_t tag_number, const uint8_t* value_bytes, size_t value_length, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic);
int server_runtime_encode_invoke_id_element(uint32_t invoke_id, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic);
int server_runtime_parse_object_reference(const char* object_reference, char* domain_id, size_t domain_id_size, char* item_id, size_t item_id_size);
const UnitLabIedModelSignal* server_runtime_find_signal_by_object_reference(const UnitLabMmsServerRuntime* server_runtime, const char* object_reference);
int server_runtime_reference_matches_signal(const char* object_reference, const UnitLabIedModelSignal* signal);
const UnitLabMmsServerRuntimeSignalValue* server_runtime_find_signal_value(const UnitLabMmsServerRuntime* server_runtime, const char* object_reference);
int server_runtime_encode_current_signal_value(const UnitLabMmsServerRuntime* server_runtime, const UnitLabIedModelSignal* signal, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic);
int server_runtime_parse_int32_value(const char* source, int32_t* value);
int server_runtime_encode_signed_integer(int32_t value, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic);
int server_runtime_encode_mms_data_value(const UnitLabIedModelSignal* signal, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic);
int server_runtime_object_reference_has_suffix(const char* object_reference, const char* suffix);
int server_runtime_object_reference_has_suffix_any(const char* object_reference, const char* const* suffixes, size_t suffix_count);
int server_runtime_object_reference_matches_report_control_field(const char* object_reference, const char* field_name);
int server_runtime_object_reference_matches_report_control_object(const char* object_reference);
const char* server_runtime_advertised_domain_name(const UnitLabMmsServerRuntime* server_runtime);

/* Browse service handles discovery, GVA, and named variable list attributes. */
int server_runtime_build_get_name_list_response_service(UnitLabMmsServerRuntime* server_runtime, uint32_t invoke_id, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic);
int server_runtime_build_get_variable_access_attributes_response_service(UnitLabMmsServerRuntime* server_runtime, uint32_t invoke_id, const char* object_reference, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic);
int server_runtime_build_get_named_variable_list_attributes_response_service(UnitLabMmsServerRuntime* server_runtime, uint32_t invoke_id, const char* object_reference, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic);

/* Read service handles ReadResponse and value lookup. */
int server_runtime_build_read_response_service(UnitLabMmsServerRuntime* server_runtime, uint32_t invoke_id, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic);

/* Write service applies confirmed Write and builds WriteResponse. */
int server_runtime_build_write_response_service(UnitLabMmsServerRuntime* server_runtime, uint32_t invoke_id, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic);

/* Report service handles BRCB state values and InformationReport encoding. */
const char* const* server_runtime_report_control_block_fields(size_t* field_count);
void server_runtime_format_report_control_references(const UnitLabMmsServerRuntime* server_runtime, char* report_id_reference, size_t report_id_reference_size, char* data_set_reference, size_t data_set_reference_size);
int server_runtime_encode_report_control_block_field_value(const UnitLabMmsServerRuntime* server_runtime, const char* field_name, const char* report_id_reference, const char* data_set_reference, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic);
int server_runtime_encode_report_control_block_value(const UnitLabMmsServerRuntime* server_runtime, const char* report_id_reference, const char* data_set_reference, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic);
int server_runtime_encode_report_control_block_container_value(const UnitLabMmsServerRuntime* server_runtime, const char* report_id_reference, const char* data_set_reference, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic);

#endif /* UNITLAB_IEC61850_IED_UNITLAB_MMS_SERVER_RUNTIME_INTERNAL_H */
