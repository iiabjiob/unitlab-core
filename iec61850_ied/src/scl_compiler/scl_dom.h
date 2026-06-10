#ifndef UNITLAB_IEC61850_SCL_DOM_H
#define UNITLAB_IEC61850_SCL_DOM_H

#include <cstdint>
#include <string>
#include <vector>

#include "fixture/fixture_parser.h"

namespace unitlab::iec61850::scl {

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
struct SclSdoTemplate { std::string name; std::string type; };
struct SclDaTemplate { std::string name; std::string fc; std::string b_type; std::string type; };
struct SclLNodeTypeTemplate { std::string id; std::vector<SclDoTemplate> data_objects; };
struct SclDoTypeTemplate { std::string id; std::vector<SclDaTemplate> data_attributes; std::vector<SclSdoTemplate> sub_data_objects; };
struct SclDaTypeTemplate { std::string id; std::vector<SclDaTemplate> basic_data_attributes; };
struct SclEnumValueTemplate { int32_t ord = 0; std::string text; };
struct SclEnumTypeTemplate { std::string id; std::string first_value; std::vector<SclEnumValueTemplate> values; };
struct SclResolvedValueType { std::string b_type; std::string type; };

struct SclDataTypeTemplates {
    std::vector<SclLNodeTypeTemplate> lnode_types;
    std::vector<SclDoTypeTemplate> do_types;
    std::vector<SclDaTypeTemplate> da_types;
    std::vector<SclEnumTypeTemplate> enum_types;
};

struct SclLogicalDevice { std::string inst; std::vector<SclLogicalNode> logical_nodes; };

struct SclAddressParameter {
    std::string type;
    std::string value;
};

struct SclConnectedAccessPoint {
    std::string ied_name;
    std::string access_point_name;
    std::string sub_network_name;
    std::string sub_network_type;
    std::vector<SclAddressParameter> address_parameters;
};

struct SclAccessPoint { std::string name; std::vector<SclLogicalDevice> logical_devices; };
struct SclIed { std::string name; std::vector<SclAccessPoint> access_points; std::vector<SclConnectedAccessPoint> connected_access_points; };
struct SclSubNetwork { std::string name; std::string type; std::vector<SclConnectedAccessPoint> connected_access_points; };

struct SclDomParseResult {
    bool ok = false;
    std::string error_code;
    std::string error_message;
    std::vector<SclIed> ieds;
    std::vector<SclSubNetwork> sub_networks;
    SclDataTypeTemplates data_type_templates;
};

SclDomParseResult parse_scl_document(const char* xml, size_t xml_size);

} // namespace unitlab::iec61850::scl

#endif /* UNITLAB_IEC61850_SCL_DOM_H */
