#ifndef UNITLAB_IEC61850_IED_UNITLAB_MMS_CORE_H
#define UNITLAB_IEC61850_IED_UNITLAB_MMS_CORE_H

#include <stddef.h>
#include <stdint.h>

typedef enum UnitLabMmsDiagnosticCode {
    UNITLAB_MMS_DIAGNOSTIC_OK = 0,
    UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT = 1,
    UNITLAB_MMS_DIAGNOSTIC_NOT_ASSOCIATED = 2,
    UNITLAB_MMS_DIAGNOSTIC_TIMEOUT = 3,
    UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR = 4,
    UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED = 5
} UnitLabMmsDiagnosticCode;

typedef struct UnitLabMmsDiagnostic {
    UnitLabMmsDiagnosticCode code;
    char message[256];
} UnitLabMmsDiagnostic;

typedef enum UnitLabMmsSessionState {
    UNITLAB_MMS_SESSION_DISCONNECTED = 0,
    UNITLAB_MMS_SESSION_ASSOCIATING = 1,
    UNITLAB_MMS_SESSION_ASSOCIATED = 2,
    UNITLAB_MMS_SESSION_RELEASING = 3,
    UNITLAB_MMS_SESSION_ABORTED = 4
} UnitLabMmsSessionState;

typedef struct UnitLabMmsSession {
    UnitLabMmsSessionState state;
    uint32_t next_invoke_id;
} UnitLabMmsSession;

typedef enum UnitLabMmsReportControlState {
    UNITLAB_MMS_REPORT_CONTROL_DISABLED = 0,
    UNITLAB_MMS_REPORT_CONTROL_RESERVED = 1,
    UNITLAB_MMS_REPORT_CONTROL_ENABLED = 2,
    UNITLAB_MMS_REPORT_CONTROL_GI_PENDING = 3,
    UNITLAB_MMS_REPORT_CONTROL_REPORTING = 4
} UnitLabMmsReportControlState;

typedef struct UnitLabMmsReportControl {
    UnitLabMmsReportControlState state;
    int gi_requested;
} UnitLabMmsReportControl;

typedef struct UnitLabMmsTransportExchange {
    const uint8_t* request_bytes;
    size_t request_length;
    uint8_t* response_bytes;
    size_t response_capacity;
    size_t response_length;
    uint32_t invoke_id;
} UnitLabMmsTransportExchange;

void unitlab_mms_diagnostic_clear(UnitLabMmsDiagnostic* diagnostic);
void unitlab_mms_session_init(UnitLabMmsSession* session);
void unitlab_mms_session_reset(UnitLabMmsSession* session);
uint32_t unitlab_mms_session_next_invoke_id(UnitLabMmsSession* session);
void unitlab_mms_report_control_init(UnitLabMmsReportControl* report_control);
void unitlab_mms_report_control_reset(UnitLabMmsReportControl* report_control);
void unitlab_mms_transport_exchange_init(UnitLabMmsTransportExchange* exchange);

#endif
