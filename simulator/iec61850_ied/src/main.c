#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fixture_parser.h"

#ifdef UNITLAB_WITH_LIBIEC61850
#include <iec61850_server.h>
#endif

typedef struct SimulatorOptions {
    const char* fixture_path;
    const char* ied_name;
    const char* bind_address;
    int port;
    int dry_run;
} SimulatorOptions;

static void print_usage(const char* program_name)
{
    printf("Usage: %s --fixture PATH --ied NAME [--bind ADDRESS] [--port PORT] [--dry-run]\n", program_name);
    printf("\n");
    printf("Options:\n");
    printf("  --fixture PATH   UnitLab IEC 61850 IED simulator fixture JSON.\n");
    printf("  --ied NAME       IED name from the fixture to expose.\n");
    printf("  --bind ADDRESS   Bind address for the future MMS server. Default: 0.0.0.0.\n");
    printf("  --port PORT      TCP port for the future MMS server. Default: 102.\n");
    printf("  --dry-run        Validate CLI and fixture boundary without opening MMS.\n");
    printf("  --help           Show this help text.\n");
}

static int parse_int(const char* value, int* out)
{
    char* end = NULL;
    long parsed = strtol(value, &end, 10);
    if (value == end || end == NULL || *end != '\0') {
        return 0;
    }
    if (parsed <= 0 || parsed > 65535) {
        return 0;
    }
    *out = (int)parsed;
    return 1;
}

static int parse_args(int argc, char** argv, SimulatorOptions* options)
{
    options->fixture_path = NULL;
    options->ied_name = NULL;
    options->bind_address = "0.0.0.0";
    options->port = 102;
    options->dry_run = 0;

    for (int index = 1; index < argc; index++) {
        const char* arg = argv[index];
        if (strcmp(arg, "--help") == 0) {
            print_usage(argv[0]);
            return 1;
        }
        if (strcmp(arg, "--dry-run") == 0) {
            options->dry_run = 1;
            continue;
        }
        if (strcmp(arg, "--fixture") == 0 && index + 1 < argc) {
            options->fixture_path = argv[++index];
            continue;
        }
        if (strcmp(arg, "--ied") == 0 && index + 1 < argc) {
            options->ied_name = argv[++index];
            continue;
        }
        if (strcmp(arg, "--bind") == 0 && index + 1 < argc) {
            options->bind_address = argv[++index];
            continue;
        }
        if (strcmp(arg, "--port") == 0 && index + 1 < argc) {
            if (!parse_int(argv[++index], &options->port)) {
                fprintf(stderr, "INVALID_PORT: expected TCP port in range 1..65535.\n");
                return -1;
            }
            continue;
        }
        fprintf(stderr, "INVALID_ARGUMENT: unsupported or incomplete argument \"%s\".\n", arg);
        return -1;
    }

    if (options->fixture_path == NULL || options->fixture_path[0] == '\0') {
        fprintf(stderr, "FIXTURE_REQUIRED: --fixture PATH is required.\n");
        return -1;
    }
    if (options->ied_name == NULL || options->ied_name[0] == '\0') {
        fprintf(stderr, "IED_REQUIRED: --ied NAME is required.\n");
        return -1;
    }
    if (options->bind_address == NULL || options->bind_address[0] == '\0') {
        fprintf(stderr, "BIND_REQUIRED: --bind ADDRESS cannot be empty.\n");
        return -1;
    }

    return 0;
}

static char* read_text_file(const char* path)
{
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "FIXTURE_OPEN_FAILED: %s: %s\n", path, strerror(errno));
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fprintf(stderr, "FIXTURE_READ_FAILED: %s: %s\n", path, strerror(errno));
        fclose(file);
        return NULL;
    }

    long length = ftell(file);
    if (length < 0) {
        fprintf(stderr, "FIXTURE_READ_FAILED: %s: %s\n", path, strerror(errno));
        fclose(file);
        return NULL;
    }
    if (length == 0) {
        fprintf(stderr, "FIXTURE_EMPTY: %s\n", path);
        fclose(file);
        return NULL;
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        fprintf(stderr, "FIXTURE_READ_FAILED: %s: %s\n", path, strerror(errno));
        fclose(file);
        return NULL;
    }

    char* buffer = (char*)malloc((size_t)length + 1U);
    if (buffer == NULL) {
        fprintf(stderr, "OUT_OF_MEMORY: cannot allocate fixture buffer.\n");
        fclose(file);
        return NULL;
    }

    size_t read_count = fread(buffer, 1U, (size_t)length, file);
    fclose(file);
    if (read_count != (size_t)length) {
        fprintf(stderr, "FIXTURE_READ_FAILED: %s\n", path);
        free(buffer);
        return NULL;
    }

    buffer[length] = '\0';
    return buffer;
}

static const char* libiec61850_status(void)
{
#ifdef UNITLAB_WITH_LIBIEC61850
    return "linked";
#else
    return "not-linked";
#endif
}

int main(int argc, char** argv)
{
    SimulatorOptions options;
    int parsed = parse_args(argc, argv, &options);
    if (parsed > 0) {
        return 0;
    }
    if (parsed < 0) {
        return 64;
    }

    char* fixture_text = read_text_file(options.fixture_path);
    if (fixture_text == NULL) {
        return 66;
    }

    UnitLabIedFixtureSummary fixture_summary;
    char fixture_error[256];
    int valid = unitlab_parse_ied_fixture_summary(
        fixture_text,
        options.ied_name,
        &fixture_summary,
        fixture_error,
        sizeof(fixture_error));
    free(fixture_text);
    if (!valid) {
        fprintf(stderr, "%s\n", fixture_error);
        return 65;
    }

    if (options.dry_run) {
        printf("unitlab-iec61850-ied-sim: fixture accepted\n");
        printf("schema=%s\n", UNITLAB_IED_SIM_SCHEMA);
        printf("ied=%s\n", options.ied_name);
        printf("accessPoint=%s\n", fixture_summary.access_point_name);
        printf("devices=%zu\n", fixture_summary.device_count);
        printf("dataSets=%zu\n", fixture_summary.data_set_count);
        printf("reports=%zu\n", fixture_summary.report_count);
        printf("signals=%zu\n", fixture_summary.signal_count);
        printf("bind=%s\n", options.bind_address);
        printf("port=%d\n", options.port);
        printf("libiec61850=%s\n", libiec61850_status());
        return 0;
    }

    fprintf(stderr, "MMS_SERVER_NOT_IMPLEMENTED: this slice only validates the external simulator process boundary.\n");
    fprintf(stderr, "libiec61850=%s\n", libiec61850_status());
    return 69;
}
