#ifndef UNITLAB_IEC61850_IED_APP_CLI_UNITLAB_CLI_INTERNAL_H
#define UNITLAB_IEC61850_IED_APP_CLI_UNITLAB_CLI_INTERNAL_H

/* Internal CLI runtime boundary. Keep public consumers on unitlab_cli.h. */

#include <stddef.h>
#include <stdint.h>

#include "app/cli/unitlab_cli_dispatch.h"
#include "app/cli/unitlab_cli_options.h"
#include "fixture/fixture_parser.h"
#include "model/model_loader.h"
#include "model/model_plan.h"
#include "server/native_wire_client.h"
#include "server/server_runtime.h"
#include "wire/ber/unitlab_mms_ber.h"
#include "wire/mms/unitlab_mms_pdu.h"
#include "wire/orchestration/unitlab_mms_association_frame.h"
#include "wire/orchestration/unitlab_mms_live_wire_probe.h"

char* unitlab_cli_read_text_file(const char* path);
const char* unitlab_cli_optional_bool_label(UnitLabIedFixtureOptionalBool field);
const char* unitlab_cli_value_kind_label(UnitLabIedFixtureValueKind value_kind);
const char* unitlab_cli_mms_backend_status(void);
int unitlab_cli_build_native_association_request_bytes(uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic);
void unitlab_cli_initialize_synthetic_fixture(UnitLabIedFixtureModel* fixture_model, const char* ied_name);
void unitlab_cli_install_stop_handlers(void);
int unitlab_cli_signal_stop_requested(void* context);
int unitlab_cli_immediate_stop_requested(void* context);

#endif /* UNITLAB_IEC61850_IED_APP_CLI_UNITLAB_CLI_INTERNAL_H */
