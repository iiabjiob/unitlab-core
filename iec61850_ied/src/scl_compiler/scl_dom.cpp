#include "scl_compiler/scl_dom.h"

#include "pugixml.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace unitlab::iec61850::scl {
namespace {

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


std::string address_parameter_value(pugi::xml_node parameter)
{
    const pugi::xml_node text = parameter.first_child();
    return text ? text.value() : "";
}

std::vector<SclAddressParameter> parse_address_parameters(pugi::xml_node connected_ap)
{
    std::vector<SclAddressParameter> parameters;
    const pugi::xml_node address = first_child(connected_ap, "Address");
    if (!address) return parameters;
    for (pugi::xml_node parameter_node : address.children()) {
        if (!is_node(parameter_node, "P")) continue;
        SclAddressParameter parameter;
        parameter.type = attr(parameter_node, "type");
        parameter.value = address_parameter_value(parameter_node);
        if (!parameter.type.empty() || !parameter.value.empty()) parameters.push_back(parameter);
    }
    return parameters;
}

std::vector<SclSubNetwork> parse_communication(pugi::xml_node root)
{
    std::vector<SclSubNetwork> sub_networks;
    const pugi::xml_node communication = first_child(root, "Communication");
    if (!communication) return sub_networks;

    for (pugi::xml_node sub_network_node : communication.children()) {
        if (!is_node(sub_network_node, "SubNetwork")) continue;
        SclSubNetwork sub_network;
        sub_network.name = attr(sub_network_node, "name");
        sub_network.type = attr(sub_network_node, "type");
        for (pugi::xml_node connected_ap_node : sub_network_node.children()) {
            if (!is_node(connected_ap_node, "ConnectedAP")) continue;
            SclConnectedAccessPoint connected_ap;
            connected_ap.ied_name = attr(connected_ap_node, "iedName");
            connected_ap.access_point_name = attr(connected_ap_node, "apName");
            connected_ap.sub_network_name = sub_network.name;
            connected_ap.sub_network_type = sub_network.type;
            connected_ap.address_parameters = parse_address_parameters(connected_ap_node);
            if (!connected_ap.ied_name.empty() || !connected_ap.access_point_name.empty()) {
                sub_network.connected_access_points.push_back(connected_ap);
            }
        }
        if (!sub_network.name.empty() || !sub_network.connected_access_points.empty()) sub_networks.push_back(sub_network);
    }
    return sub_networks;
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

std::vector<SclIed> parse_ieds(pugi::xml_node root, const std::vector<SclSubNetwork>& sub_networks)
{
    std::vector<SclIed> ieds;
    for (pugi::xml_node child : root.children()) {
        if (!is_node(child, "IED")) continue;
        SclIed ied;
        ied.name = attr(child, "name");
        ied.access_points = parse_access_points(child);
        for (const SclSubNetwork& sub_network : sub_networks) {
            for (const SclConnectedAccessPoint& connected_ap : sub_network.connected_access_points) {
                if (connected_ap.ied_name == ied.name) ied.connected_access_points.push_back(connected_ap);
            }
        }
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
        SclDaTemplate attribute{attr(child, "name"), attr(child, "fc"), attr(child, "bType"), attr(child, "type")};
        if (!attribute.name.empty()) attributes.push_back(attribute);
    }
    return attributes;
}

std::vector<SclSdoTemplate> parse_do_type_sdos(pugi::xml_node do_type)
{
    std::vector<SclSdoTemplate> objects;
    for (pugi::xml_node child : do_type.children()) {
        if (!is_node(child, "SDO")) continue;
        SclSdoTemplate object{attr(child, "name"), attr(child, "type")};
        if (!object.name.empty()) objects.push_back(object);
    }
    return objects;
}

std::vector<SclDaTemplate> parse_da_type_bdas(pugi::xml_node da_type)
{
    std::vector<SclDaTemplate> attributes;
    for (pugi::xml_node child : da_type.children()) {
        if (!is_node(child, "BDA")) continue;
        SclDaTemplate attribute{attr(child, "name"), attr(child, "fc"), attr(child, "bType"), attr(child, "type")};
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
            type.sub_data_objects = parse_do_type_sdos(child);
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

} // namespace

SclDomParseResult parse_scl_document(const char* xml, size_t xml_size)
{
    SclDomParseResult result;
    pugi::xml_document document;
    const pugi::xml_parse_result parse_result = document.load_buffer(xml, xml_size);
    if (!parse_result) {
        result.error_code = "SCL_XML_PARSE_FAILED";
        result.error_message = parse_result.description();
        return result;
    }

    const pugi::xml_node root = document.document_element();
    if (!root || !is_node(root, "SCL")) {
        result.error_code = "SCL_ROOT_MISSING";
        result.error_message = "SCL root element was not found.";
        return result;
    }

    result.ok = true;
    result.sub_networks = parse_communication(root);
    result.ieds = parse_ieds(root, result.sub_networks);
    result.data_type_templates = parse_data_type_templates(root);
    return result;
}

} // namespace unitlab::iec61850::scl
