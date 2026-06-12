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
    if (!expect_true(strcmp(unitlab_native_client_session_data_set_member_at(&session, "IED1LD0/LLN0$dsEvents", 0U), "IED1LD0/XCBR1$ST$Pos$stVal") == 0, "expected indexed DataSet first member lookup to succeed")) {
        return 1;
    }
    if (!expect_true(strcmp(unitlab_native_client_session_data_set_member_at(&session, "IED1LD0/LLN0$dsEvents", 1U), "IED1LD0/PGGIO1$ST$Ind1$stVal") == 0, "expected indexed DataSet second member lookup to succeed")) {
        return 1;
    }
    if (!expect_true(unitlab_native_client_session_data_set_member_at(&session, "IED1LD0/LLN0$dsEvents", 2U) == NULL, "expected indexed DataSet lookup to reject out-of-range member")) {
        return 1;
    }

    unitlab_native_client_session_reset(&session);
    if (!expect_true(session.discovered_data_set_count == 0U && session.discovered_data_set_member_count == 0U, "expected reset to clear DataSets and members")) {
        return 1;
    }


    for (size_t logical_index = 0U; logical_index < 150U; logical_index++) {
        char logical_device[128U];
        char logical_node[128U];
        char data_name[128U];
        snprintf(logical_device, sizeof(logical_device), "LD%03zu", logical_index);
        snprintf(logical_node, sizeof(logical_node), "LLN%03zu", logical_index);
        snprintf(data_name, sizeof(data_name), "Data%03zu", logical_index);
        if (!expect_true(unitlab_native_client_session_append_logical_device(&session, logical_device) != NULL, "expected logical device append to grow beyond initial capacity")) {
            return 1;
        }
        if (!expect_true(unitlab_native_client_session_append_logical_node(&session, logical_device, logical_node) != NULL, "expected logical node append to grow beyond initial capacity")) {
            return 1;
        }
        UnitLabNativeDiscoveredDataName* discovered_data_name = unitlab_native_client_session_append_data_name(&session, logical_device, logical_node, data_name);
        if (!expect_true(discovered_data_name != NULL, "expected data name append to grow beyond initial capacity")) {
            return 1;
        }
        unitlab_native_client_session_set_data_name_type(discovered_data_name, "structure");
        if (!expect_true(unitlab_native_client_session_append_data_component(&session, discovered_data_name, "stVal", "boolean"), "expected data component append to grow beyond initial capacity")) {
            return 1;
        }
    }
    if (!expect_true(session.discovered_logical_device_count == 150U && session.discovered_model.logical_device_count == 150U, "expected all logical devices to be retained")) {
        return 1;
    }
    if (!expect_true(session.discovered_logical_node_count == 150U && session.discovered_model.logical_node_count == 150U, "expected all logical nodes to be retained")) {
        return 1;
    }
    if (!expect_true(session.discovered_data_name_count == 150U && session.discovered_model.data_name_count == 150U, "expected all data names to be retained")) {
        return 1;
    }
    if (!expect_true(session.discovered_data_component_count == 150U && session.discovered_model.data_component_count == 150U, "expected all data components to be retained")) {
        return 1;
    }
    if (!expect_true(strcmp(session.discovered_logical_devices[149U].name, "LD149") == 0, "expected logical device lookup after growth to succeed")) {
        return 1;
    }
    if (!expect_true(strcmp(session.discovered_logical_nodes[149U].logical_device, "LD149") == 0 && strcmp(session.discovered_logical_nodes[149U].name, "LLN149") == 0, "expected logical node lookup after growth to succeed")) {
        return 1;
    }
    if (!expect_true(strcmp(session.discovered_data_names[149U].logical_node, "LLN149") == 0 && strcmp(session.discovered_data_names[149U].name, "Data149") == 0 && strcmp(session.discovered_data_names[149U].type_kind, "structure") == 0, "expected data name lookup after growth to succeed")) {
        return 1;
    }
    if (!expect_true(session.discovered_data_names[149U].component_count == 1U && strcmp(session.discovered_data_components[149U].name, "stVal") == 0 && strcmp(session.discovered_data_components[149U].type_kind, "boolean") == 0, "expected data component lookup after growth to succeed")) {
        return 1;
    }

    unitlab_native_client_session_reset(&session);
    if (!expect_true(session.discovered_logical_device_count == 0U && session.discovered_logical_node_count == 0U && session.discovered_data_name_count == 0U && session.discovered_data_component_count == 0U, "expected reset to clear logical model state")) {
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
        if (!expect_true(unitlab_native_client_session_append_leaf_ref(&session, member_reference) != NULL, "expected leaf reference append to grow beyond initial capacity")) {
            return 1;
        }
    }
    if (!expect_true(session.discovered_data_set_count == 300U, "expected all dynamic DataSets to be retained")) {
        return 1;
    }
    if (!expect_true(session.discovered_data_set_member_count == 300U, "expected all dynamic DataSet members to be retained")) {
        return 1;
    }
    if (!expect_true(session.discovered_leaf_ref_count == 300U && session.discovered_model.leaf_ref_count == 300U, "expected all dynamic leaf refs to be retained")) {
        return 1;
    }
    if (!expect_true(unitlab_native_client_session_data_set_contains_member(&session, "IED1LD0/LLN0$ds299", "IED1LD0/LLN0$ST$Member299$stVal"), "expected lookup after dynamic growth to succeed")) {
        return 1;
    }
    if (!expect_true(unitlab_native_client_session_leaf_ref_exists(&session, "IED1LD0/LLN0$ST$Member299$stVal"), "expected leaf ref lookup after dynamic growth to succeed")) {
        return 1;
    }
    if (!expect_true(strcmp(session.discovered_leaf_refs[299U].display_reference, "IED1LD0/LLN0.ST.Member299.stVal") == 0, "expected normalized display reference after growth")) {
        return 1;
    }

    {
        UnitLabNativeDiscoveredTypedDataNode* root_node;
        UnitLabNativeDiscoveredTypedDataNode* child_node;
        UnitLabNativeDiscoveredTypedDataNode* duplicate_node;

        root_node = unitlab_native_client_session_append_typed_data_node(
            &session,
            "IED1LD0",
            "LLN0",
            "ST",
            "Pos",
            "IED1LD0/LLN0$ST$Pos",
            "IED1LD0/LLN0.ST.Pos",
            "structure",
            "root",
            0U,
            (size_t)-1);
        if (!expect_true(root_node != NULL, "expected typed root node append to succeed")) {
            return 1;
        }
        child_node = unitlab_native_client_session_append_typed_data_node(
            &session,
            "IED1LD0",
            "LLN0",
            "ST",
            "Pos.stVal",
            "IED1LD0/LLN0$ST$Pos$stVal",
            "IED1LD0/LLN0.ST.Pos.stVal",
            "boolean",
            "leaf",
            1U,
            session.discovered_typed_data_node_count - 1U);
        if (!expect_true(child_node != NULL, "expected typed child node append to succeed")) {
            return 1;
        }
        duplicate_node = unitlab_native_client_session_append_typed_data_node(
            &session,
            "IED1LD0",
            "LLN0",
            "ST",
            "Pos.stVal",
            "IED1LD0/LLN0$ST$Pos$stVal",
            "IED1LD0/LLN0.ST.Pos.stVal",
            "boolean",
            "leaf",
            1U,
            session.discovered_typed_data_node_count - 1U);
        if (!expect_true(duplicate_node == child_node, "expected stable MMS refs to dedupe typed nodes")) {
            return 1;
        }
        if (!expect_true(session.discovered_typed_data_node_count == 2U && session.discovered_model.typed_data_node_count == 2U, "expected typed tree count to track appended nodes")) {
            return 1;
        }
        if (!expect_true(root_node->child_count == 1U && child_node->child_count == 0U, "expected typed tree parent/leaf counts")) {
            return 1;
        }
        if (!expect_true(strcmp(root_node->display_reference, "IED1LD0/LLN0.ST.Pos") == 0 && strcmp(child_node->mms_reference, "IED1LD0/LLN0$ST$Pos$stVal") == 0, "expected typed tree normalized refs")) {
            return 1;
        }
    }

    {
        UnitLabNativeLastReportEntry* report_entry = unitlab_native_client_session_append_last_report_entry(&session, "IED1LD0/LLN0$ST$Member299$stVal", 1, 299U);
        if (!expect_true(report_entry != NULL, "expected last report entry append to succeed")) {
            return 1;
        }
        snprintf(report_entry->value_summary, sizeof(report_entry->value_summary), "%s", "true");
        snprintf(report_entry->reason_summary, sizeof(report_entry->reason_summary), "%s", "0x0204");
        report_entry->value_kind = UNITLAB_NATIVE_REPORT_VALUE_BOOL;
        report_entry->bool_value = 1;
        report_entry->raw_tag_class = 2U;
        report_entry->raw_tag_number = 3U;
        report_entry->raw_value_length = 1U;
        report_entry->raw_reason_tag_class = 2U;
        report_entry->raw_reason_tag_number = 4U;
        report_entry->raw_reason_length = 2U;
        report_entry->reason_code = 0x0204U;
        report_entry->reason_flags = UNITLAB_NATIVE_REPORT_REASON_GENERAL_INTERROGATION;
        snprintf(report_entry->reason_labels, sizeof(report_entry->reason_labels), "%s", "general-interrogation");
        report_entry->quality_code = 0x0000U;
        snprintf(report_entry->quality_validity, sizeof(report_entry->quality_validity), "%s", "good");
        if (!expect_true(session.last_report_entry_count == 1U, "expected last report entry count")) {
            return 1;
        }
        if (!expect_true(report_entry->discovered_match && report_entry->dataset_match, "expected report entry to retain match flags")) {
            return 1;
        }
        if (!expect_true(strcmp(report_entry->display_reference, "IED1LD0/LLN0.ST.Member299.stVal") == 0, "expected report entry normalized display reference")) {
            return 1;
        }
        if (!expect_true(report_entry->inclusion_index == 299U, "expected report entry to retain Dataset inclusion index")) {
            return 1;
        }
        if (!expect_true(strcmp(report_entry->value_summary, "true") == 0 && strcmp(report_entry->reason_summary, "0x0204") == 0, "expected report entry value and reason summaries")) {
            return 1;
        }
        if (!expect_true(report_entry->value_kind == UNITLAB_NATIVE_REPORT_VALUE_BOOL && report_entry->bool_value == 1, "expected report entry typed bool value")) {
            return 1;
        }
        if (!expect_true(report_entry->raw_tag_class == 2U && report_entry->raw_tag_number == 3U && report_entry->raw_value_length == 1U, "expected report entry raw BER metadata")) {
            return 1;
        }
        if (!expect_true(report_entry->raw_reason_tag_class == 2U && report_entry->raw_reason_tag_number == 4U && report_entry->raw_reason_length == 2U && report_entry->reason_code == 0x0204U, "expected report entry raw reason metadata")) {
            return 1;
        }
        if (!expect_true((report_entry->reason_flags & UNITLAB_NATIVE_REPORT_REASON_GENERAL_INTERROGATION) != 0U && strcmp(report_entry->reason_labels, "general-interrogation") == 0, "expected report entry semantic reason metadata")) {
            return 1;
        }
        if (!expect_true(report_entry->quality_code == 0x0000U && strcmp(report_entry->quality_validity, "good") == 0, "expected report entry quality metadata")) {
            return 1;
        }
        report_entry->value_kind = UNITLAB_NATIVE_REPORT_VALUE_FLOAT;
        report_entry->floating_value = 12.5;
        report_entry->raw_tag_number = 7U;
        report_entry->raw_value_length = 5U;
        if (!expect_true(report_entry->value_kind == UNITLAB_NATIVE_REPORT_VALUE_FLOAT && report_entry->floating_value > 12.4 && report_entry->floating_value < 12.6, "expected report entry typed float value")) {
            return 1;
        }
        if (!expect_true(report_entry->raw_tag_number == 7U && report_entry->raw_value_length == 5U, "expected report entry float raw BER metadata")) {
            return 1;
        }
        unitlab_native_client_session_reset_last_report(&session);
        if (!expect_true(session.last_report_entry_count == 0U && session.discovered_model.last_report_value_count == 0U, "expected last report reset to clear mapped entries")) {
            return 1;
        }
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
