#include "unitlab_mms_core.h"

#include <string.h>

void unitlab_mms_diagnostic_clear(UnitLabMmsDiagnostic* diagnostic)
{
    if (diagnostic == NULL) {
        return;
    }
    diagnostic->code = UNITLAB_MMS_DIAGNOSTIC_OK;
    diagnostic->message[0] = '\0';
}

void unitlab_mms_session_init(UnitLabMmsSession* session)
{
    if (session == NULL) {
        return;
    }
    session->state = UNITLAB_MMS_SESSION_DISCONNECTED;
    session->next_invoke_id = 1U;
}

void unitlab_mms_session_reset(UnitLabMmsSession* session)
{
    unitlab_mms_session_init(session);
}

uint32_t unitlab_mms_session_next_invoke_id(UnitLabMmsSession* session)
{
    if (session == NULL) {
        return 0U;
    }
    uint32_t current = session->next_invoke_id;
    if (current == 0U) {
        current = 1U;
    }
    if (session->next_invoke_id == UINT32_MAX) {
        session->next_invoke_id = 1U;
    }
    else {
        session->next_invoke_id++;
        if (session->next_invoke_id == 0U) {
            session->next_invoke_id = 1U;
        }
    }
    return current;
}

void unitlab_mms_report_control_init(UnitLabMmsReportControl* report_control)
{
    if (report_control == NULL) {
        return;
    }
    report_control->state = UNITLAB_MMS_REPORT_CONTROL_DISABLED;
    report_control->gi_requested = 0;
}

void unitlab_mms_report_control_reset(UnitLabMmsReportControl* report_control)
{
    unitlab_mms_report_control_init(report_control);
}

void unitlab_mms_transport_exchange_init(UnitLabMmsTransportExchange* exchange)
{
    if (exchange == NULL) {
        return;
    }
    memset(exchange, 0, sizeof(*exchange));
}
