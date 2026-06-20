#include "app/cli/unitlab_cli_internal.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wire/ber/unitlab_mms_ber.h"
#include "wire/mms/unitlab_mms_pdu.h"
#include "wire/orchestration/unitlab_mms_association_frame.h"
#include "wire/orchestration/unitlab_mms_live_wire_probe.h"

static volatile sig_atomic_t g_running = 1;

static void handle_stop_signal(int signal_number)
{
    (void)signal_number;
    g_running = 0;
}

void unitlab_cli_install_stop_handlers(void)
{
    g_running = 1;
    signal(SIGINT, handle_stop_signal);
    signal(SIGTERM, handle_stop_signal);
}

void unitlab_cli_initialize_synthetic_fixture(UnitLabIedFixtureModel* fixture_model, const char* ied_name)
{
    if (fixture_model == NULL) {
        return;
    }
    memset(fixture_model, 0, sizeof(*fixture_model));
    snprintf(fixture_model->ied_name, sizeof(fixture_model->ied_name), "%s", ied_name);
    snprintf(fixture_model->access_point_name, sizeof(fixture_model->access_point_name), "%s", "AP1");
}

int unitlab_cli_signal_stop_requested(void* context)
{
    (void)context;
    return g_running == 0;
}

int unitlab_cli_immediate_stop_requested(void* context)
{
    (void)context;
    return 1;
}

char* unitlab_cli_read_text_file(const char* path)
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

const char* unitlab_cli_optional_bool_label(UnitLabIedFixtureOptionalBool field)
{
    if (!field.known) {
        return "unknown";
    }
    return field.value ? "true" : "false";
}

const char* unitlab_cli_value_kind_label(UnitLabIedFixtureValueKind value_kind)
{
    switch (value_kind) {
        case UNITLAB_IED_FIXTURE_VALUE_NULL:
            return "null";
        case UNITLAB_IED_FIXTURE_VALUE_BOOLEAN:
            return "boolean";
        case UNITLAB_IED_FIXTURE_VALUE_INTEGER:
            return "integer";
        case UNITLAB_IED_FIXTURE_VALUE_REAL:
            return "real";
        case UNITLAB_IED_FIXTURE_VALUE_STRING:
            return "string";
        case UNITLAB_IED_FIXTURE_VALUE_UNKNOWN:
        default:
            return "unknown";
    }
}

const char* unitlab_cli_mms_backend_status(void)
{
#ifdef UNITLAB_WITH_LIBIEC61850
    return "linked";
#else
    return "not-linked";
#endif
}

int unitlab_cli_build_native_association_request_bytes(uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic)
{
    return unitlab_mms_build_live_wire_association_request_frame(buffer, buffer_length, encoded_length, diagnostic);
}
