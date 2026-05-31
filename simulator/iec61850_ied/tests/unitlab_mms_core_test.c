#include "unitlab_mms_core.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void test_defaults(void)
{
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsSession session;
    UnitLabMmsReportControl report_control;
    UnitLabMmsTransportExchange exchange;

    memset(&diagnostic, 0xA5, sizeof(diagnostic));
    memset(&session, 0xA5, sizeof(session));
    memset(&report_control, 0xA5, sizeof(report_control));
    memset(&exchange, 0xA5, sizeof(exchange));

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_session_init(&session);
    unitlab_mms_report_control_init(&report_control);
    unitlab_mms_transport_exchange_init(&exchange);

    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(diagnostic.message[0] == '\0');
    assert(session.state == UNITLAB_MMS_SESSION_DISCONNECTED);
    assert(session.next_invoke_id == 1U);
    assert(report_control.state == UNITLAB_MMS_REPORT_CONTROL_DISABLED);
    assert(report_control.gi_requested == 0);
    assert(exchange.request_bytes == NULL);
    assert(exchange.request_length == 0U);
    assert(exchange.response_bytes == NULL);
    assert(exchange.response_capacity == 0U);
    assert(exchange.response_length == 0U);
    assert(exchange.invoke_id == 0U);
}

static void test_invoke_id_sequence(void)
{
    UnitLabMmsSession session;

    unitlab_mms_session_init(&session);
    assert(unitlab_mms_session_next_invoke_id(&session) == 1U);
    assert(unitlab_mms_session_next_invoke_id(&session) == 2U);
    session.next_invoke_id = UINT32_MAX;
    assert(unitlab_mms_session_next_invoke_id(&session) == UINT32_MAX);
    assert(session.next_invoke_id == 1U);
}

static void test_report_control_reset(void)
{
    UnitLabMmsReportControl report_control;

    report_control.state = UNITLAB_MMS_REPORT_CONTROL_REPORTING;
    report_control.gi_requested = 1;
    unitlab_mms_report_control_reset(&report_control);
    assert(report_control.state == UNITLAB_MMS_REPORT_CONTROL_DISABLED);
    assert(report_control.gi_requested == 0);
}

int main(void)
{
    test_defaults();
    test_invoke_id_sequence();
    test_report_control_reset();
    printf("unitlab-mms-core: ok\n");
    return 0;
}
