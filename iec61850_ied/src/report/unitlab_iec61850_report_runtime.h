#ifndef UNITLAB_IEC61850_IED_UNITLAB_IEC61850_REPORT_RUNTIME_H
#define UNITLAB_IEC61850_IED_UNITLAB_IEC61850_REPORT_RUNTIME_H

#include "protocols/mms/unitlab_mms_types.h"

typedef enum UnitLabIec61850ReportControlState {
    UNITLAB_IEC61850_REPORT_CONTROL_DISABLED = 0,
    UNITLAB_IEC61850_REPORT_CONTROL_RESERVED = 1,
    UNITLAB_IEC61850_REPORT_CONTROL_ENABLED = 2,
    UNITLAB_IEC61850_REPORT_CONTROL_GI_PENDING = 3,
    UNITLAB_IEC61850_REPORT_CONTROL_REPORTING = 4
} UnitLabIec61850ReportControlState;

typedef struct UnitLabIec61850ReportControl {
    UnitLabIec61850ReportControlState state;
    UnitLabMmsRuntimeEvent last_event;
    UnitLabMmsRuntimeEventLog event_log;
} UnitLabIec61850ReportControl;

void unitlab_iec61850_report_control_init(UnitLabIec61850ReportControl* report_control);
void unitlab_iec61850_report_control_reset(UnitLabIec61850ReportControl* report_control);
int unitlab_iec61850_report_control_reserve(UnitLabIec61850ReportControl* report_control, UnitLabMmsDiagnostic* diagnostic);
int unitlab_iec61850_report_control_enable(UnitLabIec61850ReportControl* report_control, UnitLabMmsDiagnostic* diagnostic);
int unitlab_iec61850_report_control_request_gi(UnitLabIec61850ReportControl* report_control, UnitLabMmsDiagnostic* diagnostic);
int unitlab_iec61850_report_control_disable(UnitLabIec61850ReportControl* report_control, UnitLabMmsDiagnostic* diagnostic);
int unitlab_iec61850_report_control_accept_report(UnitLabIec61850ReportControl* report_control, uint32_t invoke_id, UnitLabMmsDiagnostic* diagnostic);
int unitlab_iec61850_report_control_release(UnitLabIec61850ReportControl* report_control, UnitLabMmsDiagnostic* diagnostic);

#endif
