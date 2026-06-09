from __future__ import annotations

import json
from pathlib import Path

import pytest

from app.services.iec61850 import (
    Iec61850InMemorySclImportRepository,
    Iec61850SclCliCompiler,
    Iec61850SclCompilerDiagnostic,
    Iec61850SclCompilerOutput,
    Iec61850SclImportError,
    Iec61850SclImportService,
    SCL_NORMALIZED_SCHEMA,
)


class _RecordingCompiler:
    def __init__(self, output: Iec61850SclCompilerOutput) -> None:
        self.output = output
        self.calls: list[tuple[bytes, str | None]] = []

    def compile(self, source: bytes, *, selected_ied: str | None) -> Iec61850SclCompilerOutput:
        self.calls.append((source, selected_ied))
        return self.output


def test_scl_import_service_stores_source_hash_and_native_normalized_payload() -> None:
    source = b"<SCL><IED name='IED1'/></SCL>"
    compiler = _RecordingCompiler(
        Iec61850SclCompilerOutput(
            schema=SCL_NORMALIZED_SCHEMA,
            selected_ied="IED1",
            source_size=len(source),
            model={"logicalDevices": [{"inst": "IED1LD0"}], "reports": []},
            diagnostics=(Iec61850SclCompilerDiagnostic(severity="warning", code="SCL_WARN", message="diagnostic"),),
        )
    )
    repository = Iec61850InMemorySclImportRepository()
    service = Iec61850SclImportService(compiler, repository)

    record = service.import_scl(workspace_id=42, source=source, filename="station.scd", selected_ied="IED1")

    assert compiler.calls == [(source, "IED1")]
    assert record.workspace_id == 42
    assert record.source_filename == "station.scd"
    assert record.source_hash == "c1861b4369881592f37dd908af4f84672216521edef31768b523d5bb6bc23598"
    assert record.import_id == record.source_hash
    assert record.source_size == len(source)
    assert record.selected_ied == "IED1"
    assert record.normalized_schema == SCL_NORMALIZED_SCHEMA
    assert record.normalized_model["logicalDevices"][0]["inst"] == "IED1LD0"
    assert record.diagnostics[0].code == "SCL_WARN"
    assert repository.source_for(record.import_id) == source


def test_scl_import_service_rejects_schema_or_size_mismatch() -> None:
    source = b"<SCL/>"
    repository = Iec61850InMemorySclImportRepository()
    bad_schema_service = Iec61850SclImportService(
        _RecordingCompiler(Iec61850SclCompilerOutput(schema="unknown", selected_ied="IED1", source_size=len(source), model={}, diagnostics=())),
        repository,
    )

    with pytest.raises(Iec61850SclImportError) as schema_error:
        bad_schema_service.import_scl(workspace_id=1, source=source)
    assert schema_error.value.code == "SCL_SCHEMA_UNSUPPORTED"

    bad_size_service = Iec61850SclImportService(
        _RecordingCompiler(Iec61850SclCompilerOutput(schema=SCL_NORMALIZED_SCHEMA, selected_ied="IED1", source_size=999, model={}, diagnostics=())),
        repository,
    )

    with pytest.raises(Iec61850SclImportError) as size_error:
        bad_size_service.import_scl(workspace_id=1, source=source)
    assert size_error.value.code == "SCL_SOURCE_SIZE_MISMATCH"


def test_scl_cli_compiler_invokes_external_compiler_without_python_xml_parsing(tmp_path: Path) -> None:
    output = {
        "schema": SCL_NORMALIZED_SCHEMA,
        "selectedIed": "IED1",
        "sourceSize": len(b"<SCL/>"),
        "model": {"signals": [{"reference": "LD0/LLN0.Mod.stVal[ST]"}]},
        "diagnostics": [
            {
                "severity": "error",
                "code": "SCL_TEMPLATE_MISSING",
                "message": "missing template",
                "iedName": "IED1",
            }
        ],
    }
    fake_compiler = tmp_path / "fake_scl_compiler.py"
    fake_compiler.write_text(
        "#!/usr/bin/env python3\n"
        "import json, sys\n"
        "assert sys.argv[1] == '--input'\n"
        "assert sys.argv[3:] == ['--ied', 'IED1']\n"
        f"print({json.dumps(json.dumps(output))})\n"
    )
    fake_compiler.chmod(0o755)

    compiled = Iec61850SclCliCompiler(fake_compiler).compile(b"<SCL/>", selected_ied="IED1")

    assert compiled.schema == SCL_NORMALIZED_SCHEMA
    assert compiled.selected_ied == "IED1"
    assert compiled.model["signals"][0]["reference"] == "LD0/LLN0.Mod.stVal[ST]"
    assert compiled.diagnostics[0].code == "SCL_TEMPLATE_MISSING"
    assert compiled.diagnostics[0].ied_name == "IED1"


def test_scl_cli_compiler_surfaces_compiler_failures(tmp_path: Path) -> None:
    fake_compiler = tmp_path / "failing_scl_compiler.py"
    fake_compiler.write_text("#!/usr/bin/env python3\nimport sys\nprint('compile failed', file=sys.stderr)\nsys.exit(65)\n")
    fake_compiler.chmod(0o755)

    with pytest.raises(Iec61850SclImportError) as error:
        Iec61850SclCliCompiler(fake_compiler).compile(b"<SCL/>", selected_ied=None)

    assert error.value.code == "SCL_COMPILER_FAILED"
    assert "compile failed" in error.value.message
