#include "server/native_wire_client_session.h"

#include <stdio.h>
#include <string.h>

static int expect_true(int condition, const char* message)
{
    if (!condition) {
        fprintf(stderr, "%s\n", message);
        return 0;
    }
    return 1;
}

int main(void)
{
    UnitLabNativeClientSessionState session;
    UnitLabNativeDiscoveredDataSet* first;
    UnitLabNativeDiscoveredDataSet* second;
    size_t index = 99U;

    unitlab_native_client_session_reset(&session);
    first = unitlab_native_client_session_append_data_set(&session, "IED1LD0/GGIO1$dsWire");
    if (!expect_true(first != NULL, "expected first DataSet append to succeed")) {
        return 1;
    }
    snprintf(session.discovered_data_set_members[session.discovered_data_set_member_count], sizeof(session.discovered_data_set_members[session.discovered_data_set_member_count]), "%s", "IED1LD0/GGIO1$MX$AnIn1$mag$f");
    session.discovered_data_set_member_count++;
    first->member_count++;

    second = unitlab_native_client_session_append_data_set(&session, "IED1LD0/LLN0$dsEvents");
    if (!expect_true(second != NULL, "expected second DataSet append to succeed")) {
        return 1;
    }
    snprintf(session.discovered_data_set_members[session.discovered_data_set_member_count], sizeof(session.discovered_data_set_members[session.discovered_data_set_member_count]), "%s", "IED1LD0/XCBR1$ST$Pos$stVal");
    session.discovered_data_set_member_count++;
    second->member_count++;
    snprintf(session.discovered_data_set_members[session.discovered_data_set_member_count], sizeof(session.discovered_data_set_members[session.discovered_data_set_member_count]), "%s", "IED1LD0/PGGIO1$ST$Ind1$stVal");
    session.discovered_data_set_member_count++;
    second->member_count++;

    if (!expect_true(unitlab_native_client_session_data_set_index_by_reference(&session, "IED1LD0/LLN0$dsEvents", &index), "expected DataSet lookup to succeed")) {
        return 1;
    }
    if (!expect_true(index == 1U, "expected second DataSet index")) {
        return 1;
    }
    if (!expect_true(unitlab_native_client_session_data_set_member_exists(&session, "IED1LD0/XCBR1$ST$Pos$stVal"), "expected global member lookup to succeed")) {
        return 1;
    }
    if (!expect_true(unitlab_native_client_session_data_set_contains_member(&session, "IED1LD0/LLN0$dsEvents", "IED1LD0/XCBR1$ST$Pos$stVal"), "expected scoped member lookup to succeed")) {
        return 1;
    }
    if (!expect_true(!unitlab_native_client_session_data_set_contains_member(&session, "IED1LD0/GGIO1$dsWire", "IED1LD0/XCBR1$ST$Pos$stVal"), "expected scoped member lookup to reject a member from another DataSet")) {
        return 1;
    }

    unitlab_native_client_session_reset(&session);
    if (!expect_true(session.discovered_data_set_count == 0U && session.discovered_data_set_member_count == 0U, "expected reset to clear DataSets and members")) {
        return 1;
    }
    return 0;
}
