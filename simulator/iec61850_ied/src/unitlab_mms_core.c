#include "unitlab_mms_core.h"

#include <string.h>

static int set_diagnostic(UnitLabMmsDiagnostic* diagnostic, UnitLabMmsDiagnosticCode code, const char* message)
{
    if (diagnostic == NULL) {
        return 0;
    }
    diagnostic->code = code;
    if (message == NULL) {
        diagnostic->message[0] = '\0';
        return 1;
    }
    strncpy(diagnostic->message, message, sizeof(diagnostic->message) - 1U);
    diagnostic->message[sizeof(diagnostic->message) - 1U] = '\0';
    return 1;
}

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
    session->active_invoke_id = 0U;
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

int unitlab_mms_session_is_associated(const UnitLabMmsSession* session)
{
    return session != NULL && session->state == UNITLAB_MMS_SESSION_ASSOCIATED;
}

int unitlab_mms_session_begin_association(UnitLabMmsSession* session, UnitLabMmsDiagnostic* diagnostic)
{
    if (session == NULL) {
        set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "session is required for association start.");
        return 0;
    }
    if (session->state != UNITLAB_MMS_SESSION_DISCONNECTED) {
        set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "association can only begin from disconnected state.");
        return 0;
    }
    session->state = UNITLAB_MMS_SESSION_ASSOCIATING;
    session->active_invoke_id = unitlab_mms_session_next_invoke_id(session);
    set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}

int unitlab_mms_session_complete_association(UnitLabMmsSession* session, uint32_t invoke_id, UnitLabMmsDiagnostic* diagnostic)
{
    if (session == NULL) {
        set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "session is required for association completion.");
        return 0;
    }
    if (session->state != UNITLAB_MMS_SESSION_ASSOCIATING) {
        set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "association can only complete from associating state.");
        return 0;
    }
    if (session->active_invoke_id != invoke_id) {
        set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "association invoke id does not match active request.");
        return 0;
    }
    session->state = UNITLAB_MMS_SESSION_ASSOCIATED;
    set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}

int unitlab_mms_session_begin_release(UnitLabMmsSession* session, UnitLabMmsDiagnostic* diagnostic)
{
    if (session == NULL) {
        set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "session is required for release.");
        return 0;
    }
    if (session->state != UNITLAB_MMS_SESSION_ASSOCIATED) {
        set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_NOT_ASSOCIATED, "release requires an associated session.");
        return 0;
    }
    session->state = UNITLAB_MMS_SESSION_RELEASING;
    set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}

int unitlab_mms_session_abort(UnitLabMmsSession* session, UnitLabMmsDiagnostic* diagnostic)
{
    if (session == NULL) {
        set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "session is required for abort.");
        return 0;
    }
    session->state = UNITLAB_MMS_SESSION_ABORTED;
    session->active_invoke_id = 0U;
    set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
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
