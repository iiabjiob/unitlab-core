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
    UnitLabNativeClientSessionState session = {0};
    UnitLabNativeDiscoveredDataSet* first;
    UnitLabNativeDiscoveredDataSet* second;
    size_t index = 99U;

    unitlab_native_client_session_reset(&session);
    first = unitlab_native_client_session_append_data_set(&session, "IED1LD0/GGIO1$dsWire");
    if (!expect_true(first != NULL, "expected first DataSet append to succeed")) {
        return 1;
    }
    if (!expect_true(unitlab_native_client_session_append_data_set_member(&session, first, "IED1LD0/GGIO1$MX$AnIn1$mag$f"), "expected first DataSet member append to succeed")) {
        return 1;
    }

    second = unitlab_native_client_session_append_data_set(&session, "IED1LD0/LLN0$dsEvents");
    if (!expect_true(second != NULL, "expected second DataSet append to succeed")) {
        return 1;
    }
    if (!expect_true(unitlab_native_client_session_append_data_set_member(&session, second, "IED1LD0/XCBR1$ST$Pos$stVal"), "expected second DataSet first member append to succeed")) {
        return 1;
    }
    if (!expect_true(unitlab_native_client_session_append_data_set_member(&session, second, "IED1LD0/PGGIO1$ST$Ind1$stVal"), "expected second DataSet second member append to succeed")) {
        return 1;
    }

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

    for (size_t data_set_index = 0U; data_set_index < 300U; data_set_index++) {
        char data_set_reference[384U];
        char member_reference[384U];
        UnitLabNativeDiscoveredDataSet* data_set;
        snprintf(data_set_reference, sizeof(data_set_reference), "IED1LD0/LLN0$ds%03zu", data_set_index);
        snprintf(member_reference, sizeof(member_reference), "IED1LD0/LLN0$ST$Member%03zu$stVal", data_set_index);
        data_set = unitlab_native_client_session_append_data_set(&session, data_set_reference);
        if (!expect_true(data_set != NULL, "expected dynamic DataSet append to grow beyond initial capacity")) {
            return 1;
        }
        if (!expect_true(unitlab_native_client_session_append_data_set_member(&session, data_set, member_reference), "expected dynamic member append to grow beyond initial capacity")) {
            return 1;
        }
    }
    if (!expect_true(session.discovered_data_set_count == 300U, "expected all dynamic DataSets to be retained")) {
        return 1;
    }
    if (!expect_true(session.discovered_data_set_member_count == 300U, "expected all dynamic DataSet members to be retained")) {
        return 1;
    }
    if (!expect_true(unitlab_native_client_session_data_set_contains_member(&session, "IED1LD0/LLN0$ds299", "IED1LD0/LLN0$ST$Member299$stVal"), "expected lookup after dynamic growth to succeed")) {
        return 1;
    }


    for (size_t rcb_index = 0U; rcb_index < 300U; rcb_index++) {
        char item[320U];
        const UnitLabNativeDiscoveredRcb* rcb;
        snprintf(item, sizeof(item), "LLN0$BR$brcb%03zu", rcb_index);
        if (!expect_true(unitlab_native_client_session_append_discovered_rcb(&session, "IED1LD0", item) != NULL, "expected dynamic RCB append to grow beyond initial capacity")) {
            return 1;
        }
        rcb = unitlab_native_client_session_discovered_rcb_at(&session, rcb_index);
        if (!expect_true(rcb != NULL && strcmp(rcb->domain, "IED1LD0") == 0 && strcmp(rcb->item, item) == 0, "expected dynamic RCB lookup to succeed")) {
            return 1;
        }
    }
    if (!expect_true(session.discovered_rcb_count == 300U && session.discovered_model.brcb_count == 300U, "expected all dynamic RCBs to be retained")) {
        return 1;
    }

    unitlab_native_client_session_reset(&session);
    return 0;
}
