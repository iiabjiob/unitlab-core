#include "scl_compiler/unitlab_scl_compiler.h"

#include "scl_compiler/scl_dom.h"

#include "json.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

struct UnitLabSclCompileResult {
    std::string selected_ied_name;
    size_t source_size = 0U;
    mutable std::string normalized_json;
    UnitLabIedModelPlan plan{};
    std::vector<UnitLabIedModelLogicalDevice> logical_devices;
    std::vector<UnitLabIedModelLogicalNode> logical_nodes;
    std::vector<UnitLabIedModelDataSet> data_sets;
    std::vector<UnitLabIedModelReportControl> reports;
    std::vector<UnitLabIedModelSignal> signals;
    std::vector<UnitLabSclCompileDiagnostic> diagnostics;
};

namespace {

using namespace unitlab::iec61850::scl;

void copy_string(char* destination, size_t destination_size, const char* source)
{
    if (destination == nullptr || destination_size == 0U) return;
    if (source == nullptr) { destination[0] = '\0'; return; }
    std::snprintf(destination, destination_size, "%s", source);
}

void set_error(char* error, size_t error_size, const char* message)
{
    copy_string(error, error_size, message);
}

UnitLabSclCompileDiagnostic diagnostic(const char* severity, const char* code, const char* message)
{
    UnitLabSclCompileDiagnostic item{};
    copy_string(item.severity, sizeof(item.severity), severity);
    copy_string(item.code, sizeof(item.code), code);
    copy_string(item.message, sizeof(item.message), message);
    return item;
}

UnitLabSclCompileDiagnostic contextual_diagnostic(
    const char* severity,
    const char* code,
    const char* message,
    const char* ied_name,
    const char* access_point_name,
    const char* logical_device_inst,
    const char* logical_node_name,
    const char* data_set_name,
    const char* report_control_name,
    const char* member_reference)
{
    UnitLabSclCompileDiagnostic item = diagnostic(severity, code, message);
    copy_string(item.ied_name, sizeof(item.ied_name), ied_name);
    copy_string(item.access_point_name, sizeof(item.access_point_name), access_point_name);
    copy_string(item.logical_device_inst, sizeof(item.logical_device_inst), logical_device_inst);
    copy_string(item.logical_node_name, sizeof(item.logical_node_name), logical_node_name);
    copy_string(item.data_set_name, sizeof(item.data_set_name), data_set_name);
    copy_string(item.report_control_name, sizeof(item.report_control_name), report_control_name);
    copy_string(item.member_reference, sizeof(item.member_reference), member_reference);
    return item;
}

std::string ln_name(const std::string& prefix, const std::string& ln_class, const std::string& inst)
{
    if (ln_class == "LLN0") return "LLN0";
    return prefix + ln_class + inst;
}

std::string mms_domain(const std::string& ied_name, const std::string& ld_inst)
{
    return ied_name + ld_inst;
}

std::string data_set_ref(const std::string& ied_name, const std::string& access_point, const std::string& ld_inst, const std::string& ln, const std::string& name)
{
    return ied_name + "/" + access_point + "/" + ld_inst + "/" + ln + "." + name;
}

std::string signal_ref(const SclMember& member, const std::string& fallback_ld_inst)
{
    const std::string ld_inst = member.ld_inst.empty() ? fallback_ld_inst : member.ld_inst;
    std::string reference = ld_inst + "/" + ln_name(member.prefix, member.ln_class, member.ln_inst);
    if (!member.do_name.empty()) reference += "." + member.do_name;
    if (!member.da_name.empty()) reference += "." + member.da_name;
    if (!member.fc.empty()) reference += "[" + member.fc + "]";
    return reference;
}

bool is_valid_data_set_member(const SclMember& member)
{
    if (member.kind != "FCDA" && member.kind != "FCD") return false;
    if (member.ln_class.empty() || member.do_name.empty() || member.fc.empty()) return false;
    return true;
}

const SclLogicalNode* find_logical_node(const SclLogicalDevice& device, const SclMember& member, const std::string& fallback_ld_inst)
{
    const std::string member_ld = member.ld_inst.empty() ? fallback_ld_inst : member.ld_inst;
    if (member_ld != device.inst) return nullptr;
    const std::string member_ln = ln_name(member.prefix, member.ln_class, member.ln_inst);
    const auto found = std::find_if(device.logical_nodes.begin(), device.logical_nodes.end(), [&](const SclLogicalNode& node) {
        return node.name == member_ln;
    });
    return found == device.logical_nodes.end() ? nullptr : &(*found);
}

const SclLNodeTypeTemplate* find_lnode_type(const SclDataTypeTemplates& templates, const std::string& id)
{
    const auto found = std::find_if(templates.lnode_types.begin(), templates.lnode_types.end(), [&](const SclLNodeTypeTemplate& item) { return item.id == id; });
    return found == templates.lnode_types.end() ? nullptr : &(*found);
}

const SclDoTypeTemplate* find_do_type(const SclDataTypeTemplates& templates, const std::string& id)
{
    const auto found = std::find_if(templates.do_types.begin(), templates.do_types.end(), [&](const SclDoTypeTemplate& item) { return item.id == id; });
    return found == templates.do_types.end() ? nullptr : &(*found);
}

const SclDaTypeTemplate* find_da_type(const SclDataTypeTemplates& templates, const std::string& id)
{
    const auto found = std::find_if(templates.da_types.begin(), templates.da_types.end(), [&](const SclDaTypeTemplate& item) { return item.id == id; });
    return found == templates.da_types.end() ? nullptr : &(*found);
}

const SclEnumTypeTemplate* find_enum_type(const SclDataTypeTemplates& templates, const std::string& id)
{
    const auto found = std::find_if(templates.enum_types.begin(), templates.enum_types.end(), [&](const SclEnumTypeTemplate& item) { return item.id == id; });
    return found == templates.enum_types.end() ? nullptr : &(*found);
}

std::vector<std::string> split_path(const std::string& path)
{
    std::vector<std::string> parts;
    size_t pos = 0U;
    while (pos <= path.size()) {
        const size_t dot = path.find('.', pos);
        const size_t end = dot == std::string::npos ? path.size() : dot;
        if (end > pos) parts.push_back(path.substr(pos, end - pos));
        if (dot == std::string::npos) break;
        pos = dot + 1U;
    }
    return parts;
}

void append_member_diagnostic(
    UnitLabSclCompileResult& result,
    const char* code,
    const char* message,
    const SclIed& ied,
    const SclAccessPoint& access_point,
    const SclLogicalDevice& device,
    const SclLogicalNode& node,
    const SclDataSet& data_set,
    const SclMember& member)
{
    result.diagnostics.push_back(contextual_diagnostic("error", code, message, ied.name.c_str(), access_point.name.c_str(), device.inst.c_str(), node.name.c_str(), data_set.name.c_str(), "", signal_ref(member, device.inst).c_str()));
}

const char* missing_do_type_code = "SCL_TEMPLATE_DOTYPE_MISSING";

const SclDoTypeTemplate* resolve_member_do_type_with_diagnostic(
    UnitLabSclCompileResult& result,
    const SclDataTypeTemplates& templates,
    const SclIed& ied,
    const SclAccessPoint& access_point,
    const SclLogicalDevice& device,
    const SclLogicalNode& data_set_node,
    const SclDataSet& data_set,
    const SclMember& member)
{
    const SclLogicalNode* member_node = find_logical_node(device, member, device.inst);
    if (member_node == nullptr || member_node->ln_type.empty()) {
        append_member_diagnostic(result, "SCL_TEMPLATE_LNODETYPE_MISSING", "DataSet member logical node has no resolvable LNodeType.", ied, access_point, device, data_set_node, data_set, member);
        return nullptr;
    }
    const SclLNodeTypeTemplate* lnode_type = find_lnode_type(templates, member_node->ln_type);
    if (lnode_type == nullptr) {
        append_member_diagnostic(result, "SCL_TEMPLATE_LNODETYPE_MISSING", "DataSet member references a missing LNodeType template.", ied, access_point, device, data_set_node, data_set, member);
        return nullptr;
    }
    const std::vector<std::string> do_path = split_path(member.do_name);
    if (do_path.empty()) {
        append_member_diagnostic(result, "SCL_TEMPLATE_DO_MISSING", "DataSet member has an empty DO path.", ied, access_point, device, data_set_node, data_set, member);
        return nullptr;
    }
    const auto do_found = std::find_if(lnode_type->data_objects.begin(), lnode_type->data_objects.end(), [&](const SclDoTemplate& item) { return item.name == do_path.front(); });
    if (do_found == lnode_type->data_objects.end()) {
        append_member_diagnostic(result, "SCL_TEMPLATE_DO_MISSING", "DataSet member references a missing DO template.", ied, access_point, device, data_set_node, data_set, member);
        return nullptr;
    }
    if (do_found->type.empty()) {
        append_member_diagnostic(result, missing_do_type_code, "DataSet member DO template has no DOType reference.", ied, access_point, device, data_set_node, data_set, member);
        return nullptr;
    }
    const SclDoTypeTemplate* do_type = find_do_type(templates, do_found->type);
    if (do_type == nullptr) {
        append_member_diagnostic(result, missing_do_type_code, "DataSet member references a missing DOType template.", ied, access_point, device, data_set_node, data_set, member);
        return nullptr;
    }
    for (size_t index = 1U; index < do_path.size(); index++) {
        const auto sdo_found = std::find_if(do_type->sub_data_objects.begin(), do_type->sub_data_objects.end(), [&](const SclSdoTemplate& item) { return item.name == do_path[index]; });
        if (sdo_found == do_type->sub_data_objects.end()) {
            append_member_diagnostic(result, "SCL_TEMPLATE_SDO_MISSING", "DataSet member references a missing SDO template.", ied, access_point, device, data_set_node, data_set, member);
            return nullptr;
        }
        if (sdo_found->type.empty()) {
            append_member_diagnostic(result, missing_do_type_code, "DataSet member SDO template has no DOType reference.", ied, access_point, device, data_set_node, data_set, member);
            return nullptr;
        }
        do_type = find_do_type(templates, sdo_found->type);
        if (do_type == nullptr) {
            append_member_diagnostic(result, missing_do_type_code, "DataSet member SDO references a missing DOType template.", ied, access_point, device, data_set_node, data_set, member);
            return nullptr;
        }
    }
    return do_type;
}

SclResolvedValueType resolve_attribute_value_type_with_diagnostic(
    UnitLabSclCompileResult& result,
    const SclDataTypeTemplates& templates,
    const SclDoTypeTemplate* do_type,
    const std::vector<std::string>& path,
    const SclIed& ied,
    const SclAccessPoint& access_point,
    const SclLogicalDevice& device,
    const SclLogicalNode& node,
    const SclDataSet& data_set,
    const SclMember& member)
{
    if (do_type == nullptr || path.empty()) return {};
    const auto da_found = std::find_if(do_type->data_attributes.begin(), do_type->data_attributes.end(), [&](const SclDaTemplate& item) { return item.name == path.front(); });
    if (da_found == do_type->data_attributes.end()) {
        append_member_diagnostic(result, "SCL_TEMPLATE_DA_MISSING", "DataSet FCDA member references a missing DA template.", ied, access_point, device, node, data_set, member);
        return {};
    }
    if (path.size() == 1U || da_found->type.empty()) return {da_found->b_type, da_found->type};
    const SclDaTypeTemplate* da_type = find_da_type(templates, da_found->type);
    if (da_type == nullptr) {
        append_member_diagnostic(result, "SCL_TEMPLATE_DATYPE_MISSING", "DataSet FCDA member references a missing DAType template.", ied, access_point, device, node, data_set, member);
        return {};
    }
    for (size_t index = 1U; index < path.size(); index++) {
        const auto bda_found = std::find_if(da_type->basic_data_attributes.begin(), da_type->basic_data_attributes.end(), [&](const SclDaTemplate& item) { return item.name == path[index]; });
        if (bda_found == da_type->basic_data_attributes.end()) {
            append_member_diagnostic(result, "SCL_TEMPLATE_BDA_MISSING", "DataSet FCDA member references a missing BDA template.", ied, access_point, device, node, data_set, member);
            return {};
        }
        if (index + 1U == path.size()) return {bda_found->b_type, bda_found->type};
        if (bda_found->type.empty()) {
            append_member_diagnostic(result, "SCL_TEMPLATE_DATYPE_MISSING", "DataSet FCDA member nested BDA has no DAType reference.", ied, access_point, device, node, data_set, member);
            return {};
        }
        da_type = find_da_type(templates, bda_found->type);
        if (da_type == nullptr) {
            append_member_diagnostic(result, "SCL_TEMPLATE_DATYPE_MISSING", "DataSet FCDA member nested BDA references a missing DAType template.", ied, access_point, device, node, data_set, member);
            return {};
        }
    }
    return {};
}

bool resolved_enum_type_missing(const SclDataTypeTemplates& templates, const SclResolvedValueType& resolved)
{
    return resolved.b_type == "Enum" && !resolved.type.empty() && find_enum_type(templates, resolved.type) == nullptr;
}

UnitLabIedFixtureValueKind value_kind_for_b_type(const std::string& b_type)
{
    if (b_type == "BOOLEAN") return UNITLAB_IED_FIXTURE_VALUE_BOOLEAN;
    if (b_type == "FLOAT32" || b_type == "FLOAT64") return UNITLAB_IED_FIXTURE_VALUE_REAL;
    if (b_type.rfind("VisString", 0U) == 0 || b_type.rfind("Unicode", 0U) == 0 || b_type == "ObjRef" || b_type == "Timestamp" || b_type == "EntryTime") return UNITLAB_IED_FIXTURE_VALUE_STRING;
    return UNITLAB_IED_FIXTURE_VALUE_INTEGER;
}

std::string default_value_for_resolved_type(const SclDataTypeTemplates& templates, const SclResolvedValueType& resolved, UnitLabIedFixtureValueKind kind)
{
    if (resolved.b_type == "Enum" && !resolved.type.empty()) {
        const SclEnumTypeTemplate* enum_type = find_enum_type(templates, resolved.type);
        if (enum_type != nullptr && !enum_type->first_value.empty()) return enum_type->first_value;
    }
    switch (kind) {
        case UNITLAB_IED_FIXTURE_VALUE_BOOLEAN: return "false";
        case UNITLAB_IED_FIXTURE_VALUE_REAL: return "0.0";
        case UNITLAB_IED_FIXTURE_VALUE_STRING: return "";
        case UNITLAB_IED_FIXTURE_VALUE_INTEGER:
        default: return "0";
    }
}

uint8_t trigger_mask(const UnitLabIedFixtureTriggerOptions& options)
{
    uint8_t mask = 0U;
    if (options.data_change.known && options.data_change.value) mask |= UNITLAB_IED_MODEL_TRG_OPT_DATA_CHANGED;
    if (options.quality_change.known && options.quality_change.value) mask |= UNITLAB_IED_MODEL_TRG_OPT_QUALITY_CHANGED;
    if (options.data_update.known && options.data_update.value) mask |= UNITLAB_IED_MODEL_TRG_OPT_DATA_UPDATE;
    if (options.periodic.known && options.periodic.value) mask |= UNITLAB_IED_MODEL_TRG_OPT_INTEGRITY;
    if (options.general_interrogation.known && options.general_interrogation.value) mask |= UNITLAB_IED_MODEL_TRG_OPT_GI;
    return mask;
}

uint8_t optional_fields_mask(const UnitLabIedFixtureOptionalFields& fields)
{
    uint8_t mask = 0U;
    if (fields.sequence_number.known && fields.sequence_number.value) mask |= UNITLAB_IED_MODEL_RPT_OPT_SEQ_NUM;
    if (fields.timestamp.known && fields.timestamp.value) mask |= UNITLAB_IED_MODEL_RPT_OPT_TIME_STAMP;
    if (fields.reason_code.known && fields.reason_code.value) mask |= UNITLAB_IED_MODEL_RPT_OPT_REASON_FOR_INCLUSION;
    if (fields.data_set_name.known && fields.data_set_name.value) mask |= UNITLAB_IED_MODEL_RPT_OPT_DATA_SET;
    if (fields.data_reference.known && fields.data_reference.value) mask |= UNITLAB_IED_MODEL_RPT_OPT_DATA_REFERENCE;
    if (fields.buffer_overflow.known && fields.buffer_overflow.value) mask |= UNITLAB_IED_MODEL_RPT_OPT_BUFFER_OVERFLOW;
    if (fields.entry_id.known && fields.entry_id.value) mask |= UNITLAB_IED_MODEL_RPT_OPT_ENTRY_ID;
    if (fields.config_revision.known && fields.config_revision.value) mask |= UNITLAB_IED_MODEL_RPT_OPT_CONF_REV;
    return mask;
}

std::string member_reference_with_attribute(const SclMember& member, const std::string& fallback_ld_inst, const std::string& attribute_path)
{
    SclMember attributed = member;
    attributed.da_name = attribute_path;
    return signal_ref(attributed, fallback_ld_inst);
}

std::string data_set_entry_variable_ref(const std::string& member_domain, const std::string& member_ln, const std::string& fc, const std::string& data_object_name, const std::string& attribute_path)
{
    std::string object_path = data_object_name;
    if (!attribute_path.empty()) object_path += "." + attribute_path;
    std::replace(object_path.begin(), object_path.end(), '.', '$');
    return member_domain + "/" + member_ln + "$" + fc + "$" + object_path;
}

void append_compiled_signal(
    UnitLabSclCompileResult& result,
    size_t data_set_index,
    size_t member_index,
    const std::string& member_domain,
    const std::string& member_ln,
    const SclMember& member,
    const std::string& fallback_ld_inst,
    const std::string& attribute_path,
    const SclResolvedValueType& resolved_type,
    const SclDataTypeTemplates& templates)
{
    UnitLabIedModelSignal signal{};
    const std::string reference = attribute_path.empty()
        ? signal_ref(member, fallback_ld_inst)
        : member_reference_with_attribute(member, fallback_ld_inst, attribute_path);
    const std::string object_reference = member_domain + "." + member_ln + "." + member.do_name + (attribute_path.empty() ? "" : "." + attribute_path);
    copy_string(signal.reference, sizeof(signal.reference), reference.c_str());
    copy_string(signal.kind, sizeof(signal.kind), member.kind.c_str());
    signal.data_set_index = data_set_index;
    signal.member_index = member_index;
    copy_string(signal.logical_device_inst, sizeof(signal.logical_device_inst), member_domain.c_str());
    copy_string(signal.logical_node_name, sizeof(signal.logical_node_name), member_ln.c_str());
    copy_string(signal.data_object_name, sizeof(signal.data_object_name), member.do_name.c_str());
    copy_string(signal.data_attribute_path, sizeof(signal.data_attribute_path), attribute_path.c_str());
    copy_string(signal.object_reference, sizeof(signal.object_reference), object_reference.c_str());
    const std::string data_set_entry_variable = data_set_entry_variable_ref(member_domain, member_ln, member.fc, member.do_name, attribute_path);
    copy_string(signal.data_set_entry_variable, sizeof(signal.data_set_entry_variable), data_set_entry_variable.c_str());
    signal.data_set_entry_component_known = !attribute_path.empty() ? 1 : 0;
    copy_string(signal.data_set_entry_component, sizeof(signal.data_set_entry_component), attribute_path.c_str());
    copy_string(signal.fc, sizeof(signal.fc), member.fc.c_str());
    signal.initial_value_kind = value_kind_for_b_type(resolved_type.b_type);
    const std::string default_value = default_value_for_resolved_type(templates, resolved_type, signal.initial_value_kind);
    copy_string(signal.initial_value, sizeof(signal.initial_value), default_value.c_str());
    result.signals.push_back(signal);
}

void sync_plan(UnitLabSclCompileResult& result)
{
    result.plan.logical_device_count = result.logical_devices.size();
    result.plan.logical_devices = result.logical_devices.empty() ? nullptr : result.logical_devices.data();
    result.plan.logical_node_count = result.logical_nodes.size();
    result.plan.logical_nodes = result.logical_nodes.empty() ? nullptr : result.logical_nodes.data();
    result.plan.data_set_count = result.data_sets.size();
    result.plan.data_sets = result.data_sets.empty() ? nullptr : result.data_sets.data();
    result.plan.report_count = result.reports.size();
    result.plan.reports = result.reports.empty() ? nullptr : result.reports.data();
    result.plan.signal_count = result.signals.size();
    result.plan.signals = result.signals.empty() ? nullptr : result.signals.data();
    result.plan.namespace_attribute_count = 0U;
    result.plan.namespace_attributes = nullptr;
}

void append_logical_node_once(UnitLabSclCompileResult& result, const std::string& domain, const std::string& node_name)
{
    const auto found = std::find_if(result.logical_nodes.begin(), result.logical_nodes.end(), [&](const UnitLabIedModelLogicalNode& existing) {
        return domain == existing.logical_device_inst && node_name == existing.name;
    });
    if (found != result.logical_nodes.end()) return;
    UnitLabIedModelLogicalNode node{};
    copy_string(node.logical_device_inst, sizeof(node.logical_device_inst), domain.c_str());
    copy_string(node.name, sizeof(node.name), node_name.c_str());
    result.logical_nodes.push_back(node);
}

void compile_ied(UnitLabSclCompileResult& result, const SclIed& ied, const SclDataTypeTemplates& templates)
{
    for (const SclAccessPoint& access_point : ied.access_points) {
        for (const SclLogicalDevice& device : access_point.logical_devices) {
            const std::string domain = mms_domain(ied.name, device.inst);
            UnitLabIedModelLogicalDevice logical_device{};
            copy_string(logical_device.inst, sizeof(logical_device.inst), domain.c_str());
            result.logical_devices.push_back(logical_device);

            for (const SclLogicalNode& node : device.logical_nodes) {
                append_logical_node_once(result, domain, node.name);
                for (const SclDataSet& data_set : node.data_sets) {
                    const size_t data_set_index = result.data_sets.size();
                    UnitLabIedModelDataSet compiled_data_set{};
                    const std::string ref = data_set_ref(ied.name, access_point.name, device.inst, node.name, data_set.name);
                    copy_string(compiled_data_set.reference, sizeof(compiled_data_set.reference), ref.c_str());
                    copy_string(compiled_data_set.logical_device_inst, sizeof(compiled_data_set.logical_device_inst), domain.c_str());
                    copy_string(compiled_data_set.logical_node_name, sizeof(compiled_data_set.logical_node_name), node.name.c_str());
                    copy_string(compiled_data_set.name, sizeof(compiled_data_set.name), data_set.name.c_str());
                    compiled_data_set.first_signal_index = result.signals.size();
                    compiled_data_set.member_count = 0U;
                    result.data_sets.push_back(compiled_data_set);

                    size_t valid_member_index = 0U;
                    for (const SclMember& member : data_set.members) {
                        if (!is_valid_data_set_member(member)) {
                            result.diagnostics.push_back(contextual_diagnostic("error", "SCL_DATASET_MEMBER_INVALID", "DataSet member is missing required FCDA/FCD attributes.", ied.name.c_str(), access_point.name.c_str(), device.inst.c_str(), node.name.c_str(), data_set.name.c_str(), "", signal_ref(member, device.inst).c_str()));
                            continue;
                        }
                        const std::string member_ld = member.ld_inst.empty() ? device.inst : member.ld_inst;
                        const std::string member_domain = mms_domain(ied.name, member_ld);
                        const std::string member_ln = ln_name(member.prefix, member.ln_class, member.ln_inst);
                        append_logical_node_once(result, member_domain, member_ln);

                        const size_t diagnostics_before_resolution = result.diagnostics.size();
                        const SclDoTypeTemplate* member_do_type = resolve_member_do_type_with_diagnostic(result, templates, ied, access_point, device, node, data_set, member);
                        if (member_do_type == nullptr) continue;
                        if (member.kind == "FCD" || member.da_name.empty()) {
                            size_t emitted_attribute_count = 0U;
                            for (const SclDaTemplate& attribute : member_do_type->data_attributes) {
                                if (!attribute.fc.empty() && attribute.fc != member.fc) continue;
                                const SclResolvedValueType attribute_type{attribute.b_type, attribute.type};
                                if (resolved_enum_type_missing(templates, attribute_type)) {
                                    append_member_diagnostic(result, "SCL_TEMPLATE_ENUMTYPE_MISSING", "DataSet DO-level member attribute references a missing EnumType template.", ied, access_point, device, node, data_set, member);
                                    break;
                                }
                                append_compiled_signal(result, data_set_index, valid_member_index, member_domain, member_ln, member, device.inst, attribute.name, attribute_type, templates);
                                valid_member_index++;
                                emitted_attribute_count++;
                            }
                            if (emitted_attribute_count != 0U || result.diagnostics.size() != diagnostics_before_resolution) continue;
                            append_member_diagnostic(result, "SCL_TEMPLATE_DA_MISSING", "DataSet DO-level member DOType has no DA matching the requested FC.", ied, access_point, device, node, data_set, member);
                            continue;
                        }

                        const SclResolvedValueType resolved_type = resolve_attribute_value_type_with_diagnostic(result, templates, member_do_type, split_path(member.da_name), ied, access_point, device, node, data_set, member);
                        if (resolved_type.b_type.empty()) continue;
                        if (resolved_enum_type_missing(templates, resolved_type)) {
                            append_member_diagnostic(result, "SCL_TEMPLATE_ENUMTYPE_MISSING", "DataSet FCDA member references a missing EnumType template.", ied, access_point, device, node, data_set, member);
                            continue;
                        }
                        append_compiled_signal(result, data_set_index, valid_member_index, member_domain, member_ln, member, device.inst, member.da_name, resolved_type, templates);
                        valid_member_index++;
                    }
                    result.data_sets[data_set_index].member_count = valid_member_index;
                }
            }

            for (const SclLogicalNode& node : device.logical_nodes) {
                for (const SclReport& report : node.reports) {
                    const auto data_set_it = std::find_if(result.data_sets.begin(), result.data_sets.end(), [&](const UnitLabIedModelDataSet& data_set) {
                        return domain == data_set.logical_device_inst && node.name == data_set.logical_node_name && report.data_set == data_set.name;
                    });
                    if (data_set_it == result.data_sets.end()) {
                        result.diagnostics.push_back(contextual_diagnostic("error", "SCL_REPORT_DATASET_MISSING", "ReportControl references a missing DataSet.", ied.name.c_str(), access_point.name.c_str(), device.inst.c_str(), node.name.c_str(), report.data_set.c_str(), report.name.c_str(), ""));
                        continue;
                    }
                    const size_t data_set_index = static_cast<size_t>(data_set_it - result.data_sets.begin());
                    UnitLabIedModelReportControl compiled_report{};
                    const std::string key = ied.name + "/" + access_point.name + "/" + device.inst + "/" + node.name + "/" + report.name + "/" + report.report_kind;
                    copy_string(compiled_report.key, sizeof(compiled_report.key), key.c_str());
                    copy_string(compiled_report.logical_device_inst, sizeof(compiled_report.logical_device_inst), domain.c_str());
                    copy_string(compiled_report.logical_node_name, sizeof(compiled_report.logical_node_name), node.name.c_str());
                    copy_string(compiled_report.name, sizeof(compiled_report.name), report.name.c_str());
                    copy_string(compiled_report.report_kind, sizeof(compiled_report.report_kind), report.report_kind.c_str());
                    compiled_report.is_buffered = report.buffered ? 1 : 0;
                    copy_string(compiled_report.rpt_id, sizeof(compiled_report.rpt_id), report.rpt_id.c_str());
                    copy_string(compiled_report.data_set_ref, sizeof(compiled_report.data_set_ref), data_set_it->reference);
                    compiled_report.data_set_index = data_set_index;
                    compiled_report.conf_rev_known = report.conf_rev_known ? 1 : 0;
                    compiled_report.conf_rev = report.conf_rev;
                    compiled_report.indexed_known = report.indexed_known ? 1 : 0;
                    compiled_report.indexed = report.indexed ? 1 : 0;
                    compiled_report.buffer_time_ms_known = report.buffer_time_known ? 1 : 0;
                    compiled_report.buffer_time_ms = report.buffer_time;
                    compiled_report.integrity_period_ms_known = report.integrity_period_known ? 1 : 0;
                    compiled_report.integrity_period_ms = report.integrity_period;
                    compiled_report.trigger_options = report.trigger_options;
                    compiled_report.optional_fields = report.optional_fields;
                    compiled_report.trigger_options_mask = trigger_mask(report.trigger_options);
                    compiled_report.optional_fields_mask = optional_fields_mask(report.optional_fields);
                    result.reports.push_back(compiled_report);
                }
            }
        }
    }
    sync_plan(result);
}


std::string normalized_json_for_result(const UnitLabSclCompileResult& result)
{
    using nlohmann::json;
    json document;
    document["schema"] = "unitlab.iec61850.scl.normalized.v1";
    document["selectedIed"] = result.selected_ied_name;
    document["sourceSize"] = result.source_size;

    json model;
    model["logicalDevices"] = json::array();
    for (size_t index = 0U; index < result.plan.logical_device_count; index++) {
        const UnitLabIedModelLogicalDevice& device = result.plan.logical_devices[index];
        model["logicalDevices"].push_back({{"inst", device.inst}});
    }
    model["logicalNodes"] = json::array();
    for (size_t index = 0U; index < result.plan.logical_node_count; index++) {
        const UnitLabIedModelLogicalNode& node = result.plan.logical_nodes[index];
        model["logicalNodes"].push_back({{"logicalDeviceInst", node.logical_device_inst}, {"name", node.name}});
    }
    model["dataSets"] = json::array();
    for (size_t index = 0U; index < result.plan.data_set_count; index++) {
        const UnitLabIedModelDataSet& data_set = result.plan.data_sets[index];
        model["dataSets"].push_back({
            {"reference", data_set.reference},
            {"logicalDeviceInst", data_set.logical_device_inst},
            {"logicalNodeName", data_set.logical_node_name},
            {"name", data_set.name},
            {"firstSignalIndex", data_set.first_signal_index},
            {"memberCount", data_set.member_count},
        });
    }
    model["reports"] = json::array();
    for (size_t index = 0U; index < result.plan.report_count; index++) {
        const UnitLabIedModelReportControl& report = result.plan.reports[index];
        model["reports"].push_back({
            {"key", report.key},
            {"logicalDeviceInst", report.logical_device_inst},
            {"logicalNodeName", report.logical_node_name},
            {"name", report.name},
            {"reportKind", report.report_kind},
            {"isBuffered", report.is_buffered != 0},
            {"rptId", report.rpt_id},
            {"dataSetRef", report.data_set_ref},
            {"dataSetIndex", report.data_set_index},
            {"confRevKnown", report.conf_rev_known != 0},
            {"confRev", report.conf_rev},
            {"indexedKnown", report.indexed_known != 0},
            {"indexed", report.indexed != 0},
            {"bufferTimeMsKnown", report.buffer_time_ms_known != 0},
            {"bufferTimeMs", report.buffer_time_ms},
            {"integrityPeriodMsKnown", report.integrity_period_ms_known != 0},
            {"integrityPeriodMs", report.integrity_period_ms},
            {"triggerOptionsMask", report.trigger_options_mask},
            {"optionalFieldsMask", report.optional_fields_mask},
        });
    }
    model["signals"] = json::array();
    for (size_t index = 0U; index < result.plan.signal_count; index++) {
        const UnitLabIedModelSignal& signal = result.plan.signals[index];
        model["signals"].push_back({
            {"reference", signal.reference},
            {"kind", signal.kind},
            {"dataSetIndex", signal.data_set_index},
            {"memberIndex", signal.member_index},
            {"logicalDeviceInst", signal.logical_device_inst},
            {"logicalNodeName", signal.logical_node_name},
            {"dataObjectName", signal.data_object_name},
            {"dataAttributePath", signal.data_attribute_path},
            {"objectReference", signal.object_reference},
            {"dataSetEntryVariable", signal.data_set_entry_variable},
            {"dataSetEntryComponentKnown", signal.data_set_entry_component_known != 0},
            {"dataSetEntryComponent", signal.data_set_entry_component},
            {"fc", signal.fc},
            {"initialValueKind", static_cast<int>(signal.initial_value_kind)},
            {"initialValue", signal.initial_value},
        });
    }
    document["model"] = model;

    document["diagnostics"] = json::array();
    for (const UnitLabSclCompileDiagnostic& diagnostic_item : result.diagnostics) {
        document["diagnostics"].push_back({
            {"severity", diagnostic_item.severity},
            {"code", diagnostic_item.code},
            {"message", diagnostic_item.message},
            {"iedName", diagnostic_item.ied_name},
            {"accessPointName", diagnostic_item.access_point_name},
            {"logicalDeviceInst", diagnostic_item.logical_device_inst},
            {"logicalNodeName", diagnostic_item.logical_node_name},
            {"dataSetName", diagnostic_item.data_set_name},
            {"reportControlName", diagnostic_item.report_control_name},
            {"memberReference", diagnostic_item.member_reference},
        });
    }
    return document.dump();
}

const std::string& normalized_json_cache(const UnitLabSclCompileResult* result)
{
    static const std::string empty_json = "";
    if (result == nullptr) return empty_json;
    if (result->normalized_json.empty()) {
        result->normalized_json = normalized_json_for_result(*result);
    }
    return result->normalized_json;
}

} // namespace

extern "C" int unitlab_scl_compile_from_memory(
    const char* xml,
    size_t xml_size,
    const char* selected_ied_name,
    UnitLabSclCompileResult** result,
    char* error,
    size_t error_size)
{
    if (result == nullptr) {
        set_error(error, error_size, "result pointer is required");
        return 0;
    }
    *result = nullptr;
    if (xml == nullptr || xml_size == 0U) {
        set_error(error, error_size, "SCL XML input is required");
        return 0;
    }

    auto compiled = new UnitLabSclCompileResult();
    compiled->source_size = xml_size;

    const SclDomParseResult dom = parse_scl_document(xml, xml_size);
    if (!dom.ok) {
        compiled->diagnostics.push_back(diagnostic("error", dom.error_code.c_str(), dom.error_message.c_str()));
        sync_plan(*compiled);
        *result = compiled;
        set_error(error, error_size, "");
        return 1;
    }

    if (dom.ieds.empty()) {
        compiled->diagnostics.push_back(diagnostic("error", "SCL_IED_MISSING", "SCL file does not contain an IED element."));
        sync_plan(*compiled);
        *result = compiled;
        set_error(error, error_size, "");
        return 1;
    }

    const SclIed* selected = nullptr;
    if (selected_ied_name != nullptr && selected_ied_name[0] != '\0') {
        const auto found = std::find_if(dom.ieds.begin(), dom.ieds.end(), [&](const SclIed& ied) { return ied.name == selected_ied_name; });
        if (found == dom.ieds.end()) {
            compiled->diagnostics.push_back(contextual_diagnostic("error", "SCL_SELECTED_IED_MISSING", "Selected IED was not found in the SCL file.", selected_ied_name, "", "", "", "", "", ""));
            compiled->selected_ied_name = selected_ied_name;
            sync_plan(*compiled);
            *result = compiled;
            set_error(error, error_size, "");
            return 1;
        }
        selected = &(*found);
    } else {
        selected = &dom.ieds.front();
    }

    compiled->selected_ied_name = selected->name;
    compile_ied(*compiled, *selected, dom.data_type_templates);
    if (compiled->plan.logical_device_count == 0U) {
        compiled->diagnostics.push_back(contextual_diagnostic("error", "SCL_SERVER_MODEL_MISSING", "Selected IED does not contain a server logical-device model.", selected->name.c_str(), "", "", "", "", "", ""));
    }

    *result = compiled;
    set_error(error, error_size, "");
    return 1;
}

extern "C" const char* unitlab_scl_compile_selected_ied_name(const UnitLabSclCompileResult* result)
{
    return result == nullptr ? "" : result->selected_ied_name.c_str();
}

extern "C" size_t unitlab_scl_compile_source_size(const UnitLabSclCompileResult* result)
{
    return result == nullptr ? 0U : result->source_size;
}

extern "C" const UnitLabIedModelPlan* unitlab_scl_compile_model_plan(const UnitLabSclCompileResult* result)
{
    return result == nullptr ? nullptr : &result->plan;
}


extern "C" size_t unitlab_scl_compile_normalized_json_size(const UnitLabSclCompileResult* result)
{
    const std::string& json = normalized_json_cache(result);
    return result == nullptr ? 0U : json.size() + 1U;
}

extern "C" int unitlab_scl_compile_normalized_json(const UnitLabSclCompileResult* result, char* buffer, size_t buffer_size, size_t* written_size)
{
    if (written_size != nullptr) *written_size = 0U;
    if (result == nullptr || buffer == nullptr || buffer_size == 0U) return 0;
    const std::string& json = normalized_json_cache(result);
    const size_t required_size = json.size() + 1U;
    if (buffer_size < required_size) return 0;
    std::memcpy(buffer, json.c_str(), required_size);
    if (written_size != nullptr) *written_size = required_size;
    return 1;
}

extern "C" size_t unitlab_scl_compile_diagnostic_count(const UnitLabSclCompileResult* result)
{
    return result == nullptr ? 0U : result->diagnostics.size();
}

extern "C" int unitlab_scl_compile_diagnostic_at(const UnitLabSclCompileResult* result, size_t index, UnitLabSclCompileDiagnostic* diagnostic_out)
{
    if (result == nullptr || diagnostic_out == nullptr || index >= result->diagnostics.size()) return 0;
    *diagnostic_out = result->diagnostics[index];
    return 1;
}

extern "C" void unitlab_scl_compile_result_free(UnitLabSclCompileResult* result)
{
    delete result;
}
