#include "scl_compiler/unitlab_scl_compiler.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

struct UnitLabSclCompileResult {
    std::string selected_ied_name;
    size_t source_size = 0U;
    std::vector<UnitLabSclCompileDiagnostic> diagnostics;
};

namespace {

void copy_string(char* destination, size_t destination_size, const char* source)
{
    if (destination == nullptr || destination_size == 0U) {
        return;
    }
    if (source == nullptr) {
        destination[0] = '\0';
        return;
    }
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

bool contains_scl_root(const std::string& xml)
{
    return xml.find("<SCL") != std::string::npos || xml.find(":SCL") != std::string::npos;
}

std::string extract_attr(const std::string& element, const char* attr_name)
{
    const std::string name(attr_name);
    const std::string double_quoted = name + "=\"";
    const std::string single_quoted = name + "='";
    size_t pos = element.find(double_quoted);
    char quote = '"';
    if (pos == std::string::npos) {
        pos = element.find(single_quoted);
        quote = '\'';
    }
    if (pos == std::string::npos) {
        return {};
    }
    pos += name.size() + 2U;
    const size_t end = element.find(quote, pos);
    if (end == std::string::npos) {
        return {};
    }
    return element.substr(pos, end - pos);
}

std::vector<std::string> collect_ied_names(const std::string& xml)
{
    std::vector<std::string> names;
    size_t pos = 0U;
    while (pos < xml.size()) {
        const size_t plain = xml.find("<IED", pos);
        const size_t namespaced = xml.find(":IED", pos);
        size_t start = std::string::npos;
        if (plain == std::string::npos) {
            start = namespaced;
        } else if (namespaced == std::string::npos) {
            start = plain;
        } else {
            start = std::min(plain, namespaced);
        }
        if (start == std::string::npos) {
            break;
        }
        const size_t element_start = xml.rfind('<', start);
        const size_t close = xml.find('>', start);
        if (element_start == std::string::npos || close == std::string::npos) {
            break;
        }
        const std::string element = xml.substr(element_start, close - element_start + 1U);
        const std::string name = extract_attr(element, "name");
        if (!name.empty()) {
            names.push_back(name);
        }
        pos = close + 1U;
    }
    return names;
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
    const std::string source(xml, xml_size);

    if (!contains_scl_root(source)) {
        compiled->diagnostics.push_back(diagnostic("error", "SCL_ROOT_MISSING", "SCL root element was not found."));
        *result = compiled;
        set_error(error, error_size, "");
        return 1;
    }

    const std::vector<std::string> ied_names = collect_ied_names(source);
    if (ied_names.empty()) {
        compiled->diagnostics.push_back(diagnostic("error", "SCL_IED_MISSING", "SCL file does not contain an IED element."));
    } else if (selected_ied_name != nullptr && selected_ied_name[0] != '\0') {
        const auto found = std::find(ied_names.begin(), ied_names.end(), selected_ied_name);
        if (found == ied_names.end()) {
            compiled->diagnostics.push_back(diagnostic("error", "SCL_SELECTED_IED_MISSING", "Selected IED was not found in the SCL file."));
            compiled->selected_ied_name = selected_ied_name;
        } else {
            compiled->selected_ied_name = *found;
        }
    } else {
        compiled->selected_ied_name = ied_names.front();
    }

    compiled->diagnostics.push_back(diagnostic(
        "warning",
        "SCL_COMPILER_MODEL_PLAN_PENDING",
        "Native SCL compiler boundary is initialized; DataTypeTemplates-to-model-plan compilation is pending."));

    *result = compiled;
    set_error(error, error_size, "");
    return 1;
}

extern "C" const char* unitlab_scl_compile_selected_ied_name(const UnitLabSclCompileResult* result)
{
    if (result == nullptr) {
        return "";
    }
    return result->selected_ied_name.c_str();
}

extern "C" size_t unitlab_scl_compile_source_size(const UnitLabSclCompileResult* result)
{
    return result == nullptr ? 0U : result->source_size;
}

extern "C" size_t unitlab_scl_compile_diagnostic_count(const UnitLabSclCompileResult* result)
{
    return result == nullptr ? 0U : result->diagnostics.size();
}

extern "C" int unitlab_scl_compile_diagnostic_at(
    const UnitLabSclCompileResult* result,
    size_t index,
    UnitLabSclCompileDiagnostic* diagnostic_out)
{
    if (result == nullptr || diagnostic_out == nullptr || index >= result->diagnostics.size()) {
        return 0;
    }
    *diagnostic_out = result->diagnostics[index];
    return 1;
}

extern "C" void unitlab_scl_compile_result_free(UnitLabSclCompileResult* result)
{
    delete result;
}
