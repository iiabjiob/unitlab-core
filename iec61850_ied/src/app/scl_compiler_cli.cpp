#include "scl_compiler/unitlab_scl_compiler.h"
#include "scl_compiler/scl_dom.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using unitlab::iec61850::scl::parse_scl_document;

namespace {

void print_usage(const char* program)
{
    std::fprintf(stderr, "Usage: %s --input PATH [--ied NAME] [--list-ieds]\n", program != nullptr ? program : "unitlab-iec61850-scl-compiler-cli");
}

bool read_file(const std::string& path, std::string& output)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return false;
    }
    output.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    return true;
}


std::string json_escape(const std::string& value)
{
    std::string escaped;
    escaped.reserve(value.size() + 8U);
    for (const char ch : value) {
        switch (ch) {
        case '\\': escaped += "\\\\"; break;
        case '"': escaped += "\\\""; break;
        case '\b': escaped += "\\b"; break;
        case '\f': escaped += "\\f"; break;
        case '\n': escaped += "\\n"; break;
        case '\r': escaped += "\\r"; break;
        case '\t': escaped += "\\t"; break;
        default:
            if (static_cast<unsigned char>(ch) < 0x20U) {
                char buffer[7];
                std::snprintf(buffer, sizeof(buffer), "\\u%04x", static_cast<unsigned char>(ch));
                escaped += buffer;
            } else {
                escaped += ch;
            }
            break;
        }
    }
    return escaped;
}

void write_ied_list_json(const char* xml, size_t xml_size)
{
    const auto parsed = parse_scl_document(xml, xml_size);
    std::cout << "{\"schema\":\"unitlab.iec61850.scl.ied-list.v1\",\"sourceSize\":" << xml_size;
    if (!parsed.ok) {
        std::cout << ",\"ieds\":[],\"diagnostics\":[{\"severity\":\"error\",\"code\":\""
                  << json_escape(parsed.error_code) << "\",\"message\":\"" << json_escape(parsed.error_message) << "\"}]}\n";
        return;
    }
    std::cout << ",\"ieds\":[";
    for (size_t index = 0U; index < parsed.ieds.size(); ++index) {
        const auto& ied = parsed.ieds[index];
        if (index > 0U) {
            std::cout << ',';
        }
        std::cout << "{\"name\":\"" << json_escape(ied.name) << "\",\"accessPointCount\":" << ied.access_points.size() << '}';
    }
    std::cout << "],\"diagnostics\":[]}" << '\n';
}

} // namespace

int main(int argc, char** argv)
{
    std::string input_path;
    std::string selected_ied;
    bool list_ieds = false;

    for (int index = 1; index < argc; index++) {
        const char* arg = argv[index];
        if (std::strcmp(arg, "--input") == 0 && index + 1 < argc) {
            input_path = argv[++index];
        } else if (std::strcmp(arg, "--ied") == 0 && index + 1 < argc) {
            selected_ied = argv[++index];
        } else if (std::strcmp(arg, "--list-ieds") == 0) {
            list_ieds = true;
        } else if (std::strcmp(arg, "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            std::fprintf(stderr, "Unknown or incomplete argument: %s\n", arg);
            print_usage(argv[0]);
            return 64;
        }
    }

    if (input_path.empty()) {
        print_usage(argv[0]);
        return 64;
    }

    std::string xml;
    if (!read_file(input_path, xml)) {
        std::fprintf(stderr, "Failed to read SCL input: %s\n", input_path.c_str());
        return 66;
    }

    if (list_ieds) {
        write_ied_list_json(xml.data(), xml.size());
        return 0;
    }

    UnitLabSclCompileResult* result = nullptr;
    char error[256] = {0};
    if (!unitlab_scl_compile_from_memory(xml.data(), xml.size(), selected_ied.empty() ? nullptr : selected_ied.c_str(), &result, error, sizeof(error))) {
        std::fprintf(stderr, "%s\n", error[0] != '\0' ? error : "SCL compile failed");
        return 65;
    }

    const size_t json_size = unitlab_scl_compile_normalized_json_size(result);
    if (json_size == 0U) {
        std::fprintf(stderr, "SCL compiler produced empty normalized JSON\n");
        unitlab_scl_compile_result_free(result);
        return 70;
    }

    std::vector<char> json_buffer(json_size + 1U, '\0');
    size_t written_size = 0U;
    if (!unitlab_scl_compile_normalized_json(result, json_buffer.data(), json_buffer.size(), &written_size)) {
        std::fprintf(stderr, "Failed to serialize normalized SCL JSON\n");
        unitlab_scl_compile_result_free(result);
        return 70;
    }

    if (written_size > 0U && json_buffer[written_size - 1U] == '\0') {
        written_size--;
    }
    std::cout.write(json_buffer.data(), static_cast<std::streamsize>(written_size));
    std::cout << '\n';
    unitlab_scl_compile_result_free(result);
    return 0;
}
