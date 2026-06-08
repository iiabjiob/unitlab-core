#ifndef UNITLAB_IEC61850_SCL_COMPILER_H
#define UNITLAB_IEC61850_SCL_COMPILER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct UnitLabSclCompileResult UnitLabSclCompileResult;

typedef struct UnitLabSclCompileDiagnostic {
    char severity[16];
    char code[64];
    char message[256];
} UnitLabSclCompileDiagnostic;

int unitlab_scl_compile_from_memory(
    const char* xml,
    size_t xml_size,
    const char* selected_ied_name,
    UnitLabSclCompileResult** result,
    char* error,
    size_t error_size);

const char* unitlab_scl_compile_selected_ied_name(const UnitLabSclCompileResult* result);
size_t unitlab_scl_compile_source_size(const UnitLabSclCompileResult* result);
size_t unitlab_scl_compile_diagnostic_count(const UnitLabSclCompileResult* result);
int unitlab_scl_compile_diagnostic_at(
    const UnitLabSclCompileResult* result,
    size_t index,
    UnitLabSclCompileDiagnostic* diagnostic);

void unitlab_scl_compile_result_free(UnitLabSclCompileResult* result);

#ifdef __cplusplus
}
#endif

#endif /* UNITLAB_IEC61850_SCL_COMPILER_H */
