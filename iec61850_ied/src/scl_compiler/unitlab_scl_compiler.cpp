#include "scl_compiler/unitlab_scl_compiler.h"

#include "pugixml.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

struct UnitLabSclCompileResult {
    std::string selected_ied_name;
    size_t source_size = 0U;
    UnitLabIedModelPlan plan{};
    std::vector<UnitLabIedModelLogicalDevice> logical_devices;
    std::vector<UnitLabIedModelLogicalNode> logical_nodes;
    std::vector<UnitLabIedModelDataSet> data_sets;
    std::vector<UnitLabIedModelReportControl> reports;
    std::vector<UnitLabIedModelSignal> signals;
    std::vector<UnitLabSclCompileDiagnostic> diagnostics;
};

namespace {

struct SclMember {
    std::string kind;
    std::string ld_inst;
    std::string prefix;
    std::string ln_class;
    std::string ln_inst;
    std::string do_name;
    std::string da_name;
    std::string fc;
};

struct SclDataSet {
    std::string name;
    std::vector<SclMember> members;
};

struct SclReport {
    std::string name;
    std::string rpt_id;
    std::string data_set;
    std::string report_kind = "buffered";
    bool buffered = true;
    bool conf_rev_known = false;
    uint32_t conf_rev = 0U;
    bool indexed_known = false;
    bool indexed = false;
    bool buffer_time_known = false;
    uint32_t buffer_time = 0U;
    bool integrity_period_known = false;
    uint32_t integrity_period = 0U;
    UnitLabIedFixtureTriggerOptions trigger_options{};
    UnitLabIedFixtureOptionalFields optional_fields{};
};

struct SclLogicalNode {
    std::string name;
    std::string ln_type;
    std::vector<SclDataSet> data_sets;
    std::vector<SclReport> reports;
};

struct SclDoTemplate { std::string name; std::string type; };
struct SclDaTemplate { std::string name; std::string b_type; std::string type; };
struct SclLNodeTypeTemplate { std::string id; std::vector<SclDoTemplate> data_objects; };
struct SclDoTypeTemplate { std::string id; std::vector<SclDaTemplate> data_attributes; };
struct SclDaTypeTemplate { std::string id; std::vector<SclDaTemplate> basic_data_attributes; };
struct SclEnumTypeTemplate { std::string id; std::string first_value; };
struct SclResolvedValueType { std::string b_type; std::string type; };

struct SclDataTypeTemplates {
    std::vector<SclLNodeTypeTemplate> lnode_types;
    std::vector<SclDoTypeTemplate> do_types;
    std::vector<SclDaTypeTemplate> da_types;
    std::vector<SclEnumTypeTemplate> enum_types;
};

struct SclLogicalDevice { std::string inst; std::vector<SclLogicalNode> logical_nodes; };
struct SclAccessPoint { std::string name; std::vector<SclLogicalDevice> logical_devices; };
struct SclIed { std::string name; std::vector<SclAccessPoint> access_points; };

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

std::string attr(pugi::xml_node node, const char* name)
{
    const pugi::xml_attribute attribute = node.attribute(name);
    return attribute ? attribute.value() : "";
}

std::string local_name(const char* qname)
{
    if (qname == nullptr) return {};
    const char* colon = std::strrchr(qname, ':');
    return colon == nullptr ? std::string(qname) : std::string(colon + 1);
}

bool is_node(pugi::xml_node node, const char* expected)
{
    return local_name(node.name()) == expected;
}

pugi::xml_node first_child(pugi::xml_node parent, const char* expected)
{
    for (pugi::xml_node child : parent.children()) {
        if (is_node(child, expected)) return child;
    }
    return {};
}

bool parse_bool(const std::string& value, bool default_value)
{
    if (value == "true" || value == "1") return true;
    if (value == "false" || value == "0") return false;
    return default_value;
}

bool parse_u32(const std::string& value, uint32_t* out)
{
    if (value.empty() || out == nullptr) return false;
    char* end = nullptr;
    const unsigned long parsed = std::strtoul(value.c_str(), &end, 10);
    if (end == value.c_str() || *end != '\0') return false;
    *out = static_cast<uint32_t>(parsed);
    return true;
}

UnitLabIedFixtureOptionalBool optional_bool(pugi::xml_node node, const char* attr_name)
{
    const std::string value = attr(node, attr_name);
    if (value.empty()) return UnitLabIedFixtureOptionalBool{0, 0};
    return UnitLabIedFixtureOptionalBool{1, parse_bool(value, false) ? 1 : 0};
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
    if (member.kind == "FCDA" && member.da_name.empty()) return false;
    return true;
}

std::vector<SclMember> parse_data_set_members(pugi::xml_node data_set_node)
{
    std::vector<SclMember> members;
    for (pugi::xml_node child : data_set_node.children()) {
        const std::string kind = local_name(child.name());
        if (kind != "FCDA" && kind != "FCD") continue;
        SclMember member;
        member.kind = kind;
        member.ld_inst = attr(child, "ldInst");
        member.prefix = attr(child, "prefix");
        member.ln_class = attr(child, "lnClass");
        member.ln_inst = attr(child, "lnInst");
        member.do_name = attr(child, "doName");
        member.da_name = attr(child, "daName");
        member.fc = attr(child, "fc");
        members.push_back(member);
    }
    return members;
}

std::vector<SclDataSet> parse_data_sets(pugi::xml_node logical_node)
{
    std::vector<SclDataSet> data_sets;
    for (pugi::xml_node child : logical_node.children()) {
        if (!is_node(child, "DataSet")) continue;
        SclDataSet data_set;
        data_set.name = attr(child, "name");
        data_set.members = parse_data_set_members(child);
        if (!data_set.name.empty()) data_sets.push_back(data_set);
    }
    return data_sets;
}

SclReport parse_report(pugi::xml_node report_node)
{
    SclReport report;
    report.name = attr(report_node, "name");
    report.rpt_id = attr(report_node, "rptID");
    report.data_set = attr(report_node, "datSet");
    report.buffered = parse_bool(attr(report_node, "buffered"), true);
    report.report_kind = report.buffered ? "buffered" : "unbuffered";
    uint32_t parsed = 0U;
    report.conf_rev_known = parse_u32(attr(report_node, "confRev"), &parsed);
    report.conf_rev = parsed;
    report.indexed_known = !attr(report_node, "indexed").empty();
    report.indexed = parse_bool(attr(report_node, "indexed"), false);
    report.buffer_time_known = parse_u32(attr(report_node, "bufTime"), &parsed);
    report.buffer_time = parsed;
    report.integrity_period_known = parse_u32(attr(report_node, "intgPd"), &parsed);
    report.integrity_period = parsed;

    const pugi::xml_node trg = first_child(report_node, "TrgOps");
    if (trg) {
        report.trigger_options.data_change = optional_bool(trg, "dchg");
        report.trigger_options.quality_change = optional_bool(trg, "qchg");
        report.trigger_options.data_update = optional_bool(trg, "dupd");
        report.trigger_options.periodic = optional_bool(trg, "period");
        report.trigger_options.general_interrogation = optional_bool(trg, "gi");
    }
    const pugi::xml_node opt = first_child(report_node, "OptFields");
    if (opt) {
        report.optional_fields.sequence_number = optional_bool(opt, "seqNum");
        report.optional_fields.timestamp = optional_bool(opt, "timeStamp");
        report.optional_fields.reason_code = optional_bool(opt, "reasonCode");
        report.optional_fields.data_set_name = optional_bool(opt, "dataSet");
        report.optional_fields.data_reference = optional_bool(opt, "dataRef");
        report.optional_fields.buffer_overflow = optional_bool(opt, "bufOvfl");
        report.optional_fields.entry_id = optional_bool(opt, "entryID");
        report.optional_fields.config_revision = optional_bool(opt, "configRef");
    }
    return report;
}

std::vector<SclReport> parse_reports(pugi::xml_node logical_node)
{
    std::vector<SclReport> reports;
    for (pugi::xml_node child : logical_node.children()) {
        if (!is_node(child, "ReportControl")) continue;
        SclReport report = parse_report(child);
        if (!report.name.empty()) reports.push_back(report);
    }
    return reports;
}

std::vector<SclLogicalNode> parse_logical_nodes(pugi::xml_node ldevice)
{
    std::vector<SclLogicalNode> nodes;
    for (pugi::xml_node child : ldevice.children()) {
        if (!is_node(child, "LN0") && !is_node(child, "LN")) continue;
        SclLogicalNode node;
        if (is_node(child, "LN0")) node.name = "LLN0";
        else node.name = ln_name(attr(child, "prefix"), attr(child, "lnClass"), attr(child, "inst"));
        node.ln_type = attr(child, "lnType");
        node.data_sets = parse_data_sets(child);
        node.reports = parse_reports(child);
        if (!node.name.empty()) nodes.push_back(node);
    }
    std::stable_sort(nodes.begin(), nodes.end(), [](const SclLogicalNode& left, const SclLogicalNode& right) {
        return left.name == "LLN0" && right.name != "LLN0";
    });
    return nodes;
}

std::vector<SclLogicalDevice> parse_logical_devices(pugi::xml_node server)
{
    std::vector<SclLogicalDevice> devices;
    for (pugi::xml_node child : server.children()) {
        if (!is_node(child, "LDevice")) continue;
        SclLogicalDevice device;
        device.inst = attr(child, "inst");
        device.logical_nodes = parse_logical_nodes(child);
        if (!device.inst.empty()) devices.push_back(device);
    }
    return devices;
}

std::vector<SclAccessPoint> parse_access_points(pugi::xml_node ied)
{
    std::vector<SclAccessPoint> access_points;
    for (pugi::xml_node child : ied.children()) {
        if (!is_node(child, "AccessPoint")) continue;
        SclAccessPoint ap;
        ap.name = attr(child, "name");
        const pugi::xml_node server = first_child(child, "Server");
        if (server) ap.logical_devices = parse_logical_devices(server);
        if (!ap.name.empty()) access_points.push_back(ap);
    }
    return access_points;
}

std::vector<SclIed> parse_ieds(pugi::xml_node root)
{
    std::vector<SclIed> ieds;
    for (pugi::xml_node child : root.children()) {
        if (!is_node(child, "IED")) continue;
        SclIed ied;
        ied.name = attr(child, "name");
        ied.access_points = parse_access_points(child);
        if (!ied.name.empty()) ieds.push_back(ied);
    }
    return ieds;
}

std::vector<SclDoTemplate> parse_lnode_type_dos(pugi::xml_node lnode_type)
{
    std::vector<SclDoTemplate> objects;
    for (pugi::xml_node child : lnode_type.children()) {
        if (!is_node(child, "DO")) continue;
        SclDoTemplate object{attr(child, "name"), attr(child, "type")};
        if (!object.name.empty()) objects.push_back(object);
    }
    return objects;
}

std::vector<SclDaTemplate> parse_do_type_das(pugi::xml_node do_type)
{
    std::vector<SclDaTemplate> attributes;
    for (pugi::xml_node child : do_type.children()) {
        if (!is_node(child, "DA")) continue;
        SclDaTemplate attribute{attr(child, "name"), attr(child, "bType"), attr(child, "type")};
        if (!attribute.name.empty()) attributes.push_back(attribute);
    }
    return attributes;
}

std::vector<SclDaTemplate> parse_da_type_bdas(pugi::xml_node da_type)
{
    std::vector<SclDaTemplate> attributes;
    for (pugi::xml_node child : da_type.children()) {
        if (!is_node(child, "BDA")) continue;
        SclDaTemplate attribute{attr(child, "name"), attr(child, "bType"), attr(child, "type")};
        if (!attribute.name.empty()) attributes.push_back(attribute);
    }
    return attributes;
}

std::string parse_enum_type_first_value(pugi::xml_node enum_type)
{
    for (pugi::xml_node child : enum_type.children()) {
        if (!is_node(child, "EnumVal")) continue;
        std::string value = attr(child, "ord");
        if (value.empty()) value = attr(child, "value");
        return value;
    }
    return {};
}

SclDataTypeTemplates parse_data_type_templates(pugi::xml_node root)
{
    SclDataTypeTemplates templates;
    const pugi::xml_node dtt = first_child(root, "DataTypeTemplates");
    if (!dtt) return templates;
    for (pugi::xml_node child : dtt.children()) {
        if (is_node(child, "LNodeType")) {
            SclLNodeTypeTemplate type;
            type.id = attr(child, "id");
            type.data_objects = parse_lnode_type_dos(child);
            if (!type.id.empty()) templates.lnode_types.push_back(type);
        } else if (is_node(child, "DOType")) {
            SclDoTypeTemplate type;
            type.id = attr(child, "id");
            type.data_attributes = parse_do_type_das(child);
            if (!type.id.empty()) templates.do_types.push_back(type);
        } else if (is_node(child, "DAType")) {
            SclDaTypeTemplate type;
            type.id = attr(child, "id");
            type.basic_data_attributes = parse_da_type_bdas(child);
            if (!type.id.empty()) templates.da_types.push_back(type);
        } else if (is_node(child, "EnumType")) {
            SclEnumTypeTemplate type;
            type.id = attr(child, "id");
            type.first_value = parse_enum_type_first_value(child);
            if (!type.id.empty()) templates.enum_types.push_back(type);
        }
    }
    return templates;
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

SclResolvedValueType resolve_member_value_type(const SclDataTypeTemplates& templates, const SclLogicalDevice& device, const SclMember& member)
{
    if (member.kind != "FCDA" || member.da_name.empty()) return {};
    const SclLogicalNode* node = find_logical_node(device, member, device.inst);
    if (node == nullptr || node->ln_type.empty()) return {};
    const SclLNodeTypeTemplate* lnode_type = find_lnode_type(templates, node->ln_type);
    if (lnode_type == nullptr) return {};
    const auto do_found = std::find_if(lnode_type->data_objects.begin(), lnode_type->data_objects.end(), [&](const SclDoTemplate& item) { return item.name == member.do_name; });
    if (do_found == lnode_type->data_objects.end() || do_found->type.empty()) return {};
    const SclDoTypeTemplate* do_type = find_do_type(templates, do_found->type);
    if (do_type == nullptr) return {};
    const std::vector<std::string> path = split_path(member.da_name);
    if (path.empty()) return {};
    const auto da_found = std::find_if(do_type->data_attributes.begin(), do_type->data_attributes.end(), [&](const SclDaTemplate& item) { return item.name == path.front(); });
    if (da_found == do_type->data_attributes.end()) return {};
    if (path.size() == 1U || da_found->type.empty()) return {da_found->b_type, da_found->type};
    const SclDaTypeTemplate* da_type = find_da_type(templates, da_found->type);
    for (size_t index = 1U; da_type != nullptr && index < path.size(); index++) {
        const auto bda_found = std::find_if(da_type->basic_data_attributes.begin(), da_type->basic_data_attributes.end(), [&](const SclDaTemplate& item) { return item.name == path[index]; });
        if (bda_found == da_type->basic_data_attributes.end()) return {};
        if (index + 1U == path.size()) return {bda_found->b_type, bda_found->type};
        da_type = bda_found->type.empty() ? nullptr : find_da_type(templates, bda_found->type);
    }
    return {};
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

                        UnitLabIedModelSignal signal{};
                        copy_string(signal.reference, sizeof(signal.reference), signal_ref(member, device.inst).c_str());
                        copy_string(signal.kind, sizeof(signal.kind), member.kind.c_str());
                        signal.data_set_index = data_set_index;
                        signal.member_index = valid_member_index;
                        copy_string(signal.logical_device_inst, sizeof(signal.logical_device_inst), member_domain.c_str());
                        copy_string(signal.logical_node_name, sizeof(signal.logical_node_name), member_ln.c_str());
                        copy_string(signal.data_object_name, sizeof(signal.data_object_name), member.do_name.c_str());
                        copy_string(signal.data_attribute_path, sizeof(signal.data_attribute_path), member.da_name.c_str());
                        const std::string object_reference = member_domain + "." + member_ln + "." + member.do_name + (member.da_name.empty() ? "" : "." + member.da_name);
                        copy_string(signal.object_reference, sizeof(signal.object_reference), object_reference.c_str());
                        copy_string(signal.data_set_entry_variable, sizeof(signal.data_set_entry_variable), object_reference.c_str());
                        signal.data_set_entry_component_known = !member.da_name.empty() ? 1 : 0;
                        copy_string(signal.data_set_entry_component, sizeof(signal.data_set_entry_component), member.da_name.c_str());
                        copy_string(signal.fc, sizeof(signal.fc), member.fc.c_str());
                        const SclResolvedValueType resolved_type = resolve_member_value_type(templates, device, member);
                        signal.initial_value_kind = value_kind_for_b_type(resolved_type.b_type);
                        const std::string default_value = default_value_for_resolved_type(templates, resolved_type, signal.initial_value_kind);
                        copy_string(signal.initial_value, sizeof(signal.initial_value), default_value.c_str());
                        result.signals.push_back(signal);
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

    pugi::xml_document document;
    const pugi::xml_parse_result parse_result = document.load_buffer(xml, xml_size);
    if (!parse_result) {
        compiled->diagnostics.push_back(diagnostic("error", "SCL_XML_PARSE_FAILED", parse_result.description()));
        sync_plan(*compiled);
        *result = compiled;
        set_error(error, error_size, "");
        return 1;
    }

    pugi::xml_node root = document.document_element();
    if (!root || !is_node(root, "SCL")) {
        compiled->diagnostics.push_back(diagnostic("error", "SCL_ROOT_MISSING", "SCL root element was not found."));
        sync_plan(*compiled);
        *result = compiled;
        set_error(error, error_size, "");
        return 1;
    }

    const std::vector<SclIed> ieds = parse_ieds(root);
    if (ieds.empty()) {
        compiled->diagnostics.push_back(diagnostic("error", "SCL_IED_MISSING", "SCL file does not contain an IED element."));
        sync_plan(*compiled);
        *result = compiled;
        set_error(error, error_size, "");
        return 1;
    }

    const SclIed* selected = nullptr;
    if (selected_ied_name != nullptr && selected_ied_name[0] != '\0') {
        const auto found = std::find_if(ieds.begin(), ieds.end(), [&](const SclIed& ied) { return ied.name == selected_ied_name; });
        if (found == ieds.end()) {
            compiled->diagnostics.push_back(contextual_diagnostic("error", "SCL_SELECTED_IED_MISSING", "Selected IED was not found in the SCL file.", selected_ied_name, "", "", "", "", "", ""));
            compiled->selected_ied_name = selected_ied_name;
            sync_plan(*compiled);
            *result = compiled;
            set_error(error, error_size, "");
            return 1;
        }
        selected = &(*found);
    } else {
        selected = &ieds.front();
    }

    compiled->selected_ied_name = selected->name;
    const SclDataTypeTemplates templates = parse_data_type_templates(root);
    compile_ied(*compiled, *selected, templates);
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
