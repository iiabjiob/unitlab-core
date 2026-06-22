#ifndef UNITLAB_IEC61850_IED_NATIVE_WIRE_SIGNAL_RUNTIME_H
#define UNITLAB_IEC61850_IED_NATIVE_WIRE_SIGNAL_RUNTIME_H

/* Internal signal runtime: protocol-independent live cache and change notifications. */

#include <stddef.h>
#include <stdint.h>

typedef enum {
    UNITLAB_NATIVE_SIGNAL_LEAF_ROLE_VALUE = 0,
    UNITLAB_NATIVE_SIGNAL_LEAF_ROLE_QUALITY,
    UNITLAB_NATIVE_SIGNAL_LEAF_ROLE_TIMESTAMP,
    UNITLAB_NATIVE_SIGNAL_LEAF_ROLE_OTHER
} UnitLabNativeSignalLeafRole;

typedef enum {
    UNITLAB_NATIVE_SIGNAL_VALUE_KIND_UNKNOWN = 0,
    UNITLAB_NATIVE_SIGNAL_VALUE_KIND_BOOL,
    UNITLAB_NATIVE_SIGNAL_VALUE_KIND_INTEGER,
    UNITLAB_NATIVE_SIGNAL_VALUE_KIND_UNSIGNED,
    UNITLAB_NATIVE_SIGNAL_VALUE_KIND_FLOAT,
    UNITLAB_NATIVE_SIGNAL_VALUE_KIND_STRING,
    UNITLAB_NATIVE_SIGNAL_VALUE_KIND_BIT_STRING,
    UNITLAB_NATIVE_SIGNAL_VALUE_KIND_OCTETS,
    UNITLAB_NATIVE_SIGNAL_VALUE_KIND_STRUCTURE
} UnitLabNativeSignalValueKind;

typedef struct {
    int is_new;
    int value_changed;
    int quality_changed;
    int timestamp_changed;
} UnitLabNativeSignalChange;

typedef struct {
    char signal_path[384U];
    char data_reference[384U];
    char display_reference[384U];
    char leaf_name[64U];
    char value_summary[160U];
    char quality_summary[160U];
    char timestamp_summary[160U];
    char source_session_id[128U];
    char source_endpoint_id[160U];
    char source_device_key[160U];
    char source_report_rpt_id[160U];
    char source_report_dat_set[160U];
    uint64_t source_connection_generation;
    uint64_t observed_at_ms;
    uint64_t last_changed_ms;
    uint64_t update_count;
    uint64_t version;
    UnitLabNativeSignalLeafRole leaf_role;
    UnitLabNativeSignalValueKind value_kind;
    uint64_t unsigned_value;
    int64_t integer_value;
    double floating_value;
    int bool_value;
    uint32_t quality_code;
    char quality_validity[32U];
    uint32_t reason_code;
    char reason_labels[128U];
    int has_value;
    int has_quality;
    int has_timestamp;
} UnitLabNativeSignalState;

typedef struct {
    char data_reference[384U];
    char display_reference[384U];
    char signal_path[384U];
    char leaf_name[64U];
    char value_summary[160U];
    char quality_summary[160U];
    char timestamp_summary[160U];
    char source_report_rpt_id[160U];
    char source_report_dat_set[160U];
    char reason_labels[128U];
    UnitLabNativeSignalLeafRole leaf_role;
    UnitLabNativeSignalValueKind value_kind;
    uint64_t observed_at_ms;
    uint64_t source_connection_generation;
    uint64_t unsigned_value;
    int64_t integer_value;
    double floating_value;
    int bool_value;
    uint32_t quality_code;
    char quality_validity[32U];
    uint32_t reason_code;
} UnitLabNativeSignalUpdate;

typedef void (*UnitLabNativeSignalObserver)(
    const UnitLabNativeSignalState* signal,
    const UnitLabNativeSignalChange* change,
    void* user_data);

typedef struct {
    char session_id[128U];
    char endpoint_id[160U];
    char device_key[160U];
    UnitLabNativeSignalState* items;
    size_t item_count;
    size_t item_capacity;
    uint64_t update_count;
    uint64_t change_count;
    UnitLabNativeSignalObserver observer;
    void* observer_user_data;
} UnitLabNativeSignalRuntime;

void unitlab_native_signal_runtime_init(UnitLabNativeSignalRuntime* runtime);
void unitlab_native_signal_runtime_reset(UnitLabNativeSignalRuntime* runtime);
void unitlab_native_signal_runtime_set_source_identity(
    UnitLabNativeSignalRuntime* runtime,
    const char* session_id,
    const char* endpoint_id,
    const char* device_key);
void unitlab_native_signal_runtime_set_observer(
    UnitLabNativeSignalRuntime* runtime,
    UnitLabNativeSignalObserver observer,
    void* user_data);
UnitLabNativeSignalState* unitlab_native_signal_runtime_apply_update(
    UnitLabNativeSignalRuntime* runtime,
    const UnitLabNativeSignalUpdate* update,
    UnitLabNativeSignalChange* change);
const UnitLabNativeSignalState* unitlab_native_signal_runtime_find(
    const UnitLabNativeSignalRuntime* runtime,
    const char* signal_path);

#endif /* UNITLAB_IEC61850_IED_NATIVE_WIRE_SIGNAL_RUNTIME_H */
