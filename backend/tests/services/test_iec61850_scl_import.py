from __future__ import annotations

import json
from pathlib import Path
from typing import Protocol, cast, final

import pytest
from sqlalchemy.ext.asyncio import AsyncSession

from app.services.iec61850 import (
    Iec61850InMemorySclImportRepository,
    Iec61850SclCliCompiler,
    Iec61850SclCompilerDiagnostic,
    Iec61850SclCompilerOutput,
    Iec61850SclImportError,
    Iec61850SclImportService,
    Iec61850SqlAlchemySclImportRepository,
    SCL_NORMALIZED_SCHEMA,
    create_scl_cli_compiler_from_settings,
)
from app.services.iec61850.scl_import import Iec61850SclCompiler


def _payload(value: object) -> dict[str, object]:
    return cast(dict[str, object], value)


def _payload_rows(value: object, key: str) -> list[dict[str, object]]:
    return cast(list[dict[str, object]], _payload(value)[key])


class _HasId(Protocol):
    id: int


class _SavedImportRow(Protocol):
    workspace_id: int
    source_bytes: bytes
    normalized_model: object
    diagnostics: list[dict[str, object]]


class _SavedEvent(Protocol):
    operation: str
    payload: dict[str, object]


@final
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
    service = Iec61850SclImportService(cast(Iec61850SclCompiler, cast(object, compiler)), repository)

    record = service.import_scl(workspace_id=42, source=source, filename="station.scd", selected_ied="IED1")

    assert compiler.calls == [(source, "IED1")]
    assert record.workspace_id == 42
    assert record.source_filename == "station.scd"
    assert record.source_hash == "c1861b4369881592f37dd908af4f84672216521edef31768b523d5bb6bc23598"
    assert record.import_id == record.source_hash
    assert record.source_size == len(source)
    assert record.selected_ied == "IED1"
    assert record.normalized_schema == SCL_NORMALIZED_SCHEMA
    assert _payload_rows(record.normalized_model, "logicalDevices")[0]["inst"] == "IED1LD0"
    assert record.diagnostics[0].code == "SCL_WARN"
    assert repository.source_for(record.import_id) == source


def test_scl_import_service_rejects_schema_or_size_mismatch() -> None:
    source = b"<SCL/>"
    repository = Iec61850InMemorySclImportRepository()
    bad_schema_service = Iec61850SclImportService(
        cast(Iec61850SclCompiler, cast(object, _RecordingCompiler(Iec61850SclCompilerOutput(schema="unknown", selected_ied="IED1", source_size=len(source), model={}, diagnostics=())))),
        repository,
    )

    with pytest.raises(Iec61850SclImportError) as schema_error:
        _ = bad_schema_service.import_scl(workspace_id=1, source=source)
    assert schema_error.value.code == "SCL_SCHEMA_UNSUPPORTED"

    bad_size_service = Iec61850SclImportService(
        cast(Iec61850SclCompiler, cast(object, _RecordingCompiler(Iec61850SclCompilerOutput(schema=SCL_NORMALIZED_SCHEMA, selected_ied="IED1", source_size=999, model={}, diagnostics=())))),
        repository,
    )

    with pytest.raises(Iec61850SclImportError) as size_error:
        _ = bad_size_service.import_scl(workspace_id=1, source=source)
    assert size_error.value.code == "SCL_SOURCE_SIZE_MISMATCH"




def test_scl_cli_compiler_discovers_ieds_without_python_xml_parsing(tmp_path: Path) -> None:
    output = {
        "schema": "unitlab.iec61850.scl.ied-list.v1",
        "sourceSize": len(b"<SCL/>"),
        "ieds": [{"name": "IED1", "accessPointCount": 2}, {"name": "IED2", "accessPointCount": 1}],
        "diagnostics": [],
    }
    fake_compiler = tmp_path / "fake_scl_compiler.py"
    _ = fake_compiler.write_text(
        "#!/usr/bin/env python3\n" +
        "import json, sys\n" +
        "assert sys.argv[1] == '--input'\n" +
        "assert sys.argv[3:] == ['--list-ieds']\n" +
        f"print({json.dumps(json.dumps(output))})\n"
    )
    fake_compiler.chmod(0o755)

    discovered = Iec61850SclCliCompiler(fake_compiler).discover_ieds(b"<SCL/>")

    assert discovered.schema == "unitlab.iec61850.scl.ied-list.v1"
    assert [ied.name for ied in discovered.ieds] == ["IED1", "IED2"]
    assert discovered.ieds[0].access_point_count == 2

def test_scl_cli_compiler_settings_uses_dev_build_fallback(monkeypatch: pytest.MonkeyPatch) -> None:
    from app.core.config import get_settings

    monkeypatch.delenv("IEC61850_SCL_COMPILER_BINARY_PATH", raising=False)
    monkeypatch.setenv("APP_ENV", "development")
    get_settings.cache_clear()
    try:
        compiler = create_scl_cli_compiler_from_settings()
    finally:
        get_settings.cache_clear()

    assert compiler.binary_path.name == "unitlab-iec61850-scl-compiler-cli"

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
    _ = fake_compiler.write_text(
        "#!/usr/bin/env python3\n" +
        "import json, sys\n" +
        "assert sys.argv[1] == '--input'\n" +
        "assert sys.argv[3:] == ['--ied', 'IED1']\n" +
        f"print({json.dumps(json.dumps(output))})\n"
    )
    fake_compiler.chmod(0o755)

    compiled = Iec61850SclCliCompiler(fake_compiler).compile(b"<SCL/>", selected_ied="IED1")

    assert compiled.schema == SCL_NORMALIZED_SCHEMA
    assert compiled.selected_ied == "IED1"
    assert _payload_rows(compiled.model, "signals")[0]["reference"] == "LD0/LLN0.Mod.stVal[ST]"
    assert compiled.diagnostics[0].code == "SCL_TEMPLATE_MISSING"
    assert compiled.diagnostics[0].ied_name == "IED1"


def test_scl_cli_compiler_surfaces_compiler_failures(tmp_path: Path) -> None:
    fake_compiler = tmp_path / "failing_scl_compiler.py"
    _ = fake_compiler.write_text("#!/usr/bin/env python3\nimport sys\nprint('compile failed', file=sys.stderr)\nsys.exit(65)\n")
    fake_compiler.chmod(0o755)

    with pytest.raises(Iec61850SclImportError) as error:
        _ = Iec61850SclCliCompiler(fake_compiler).compile(b"<SCL/>", selected_ied=None)

    assert error.value.code == "SCL_COMPILER_FAILED"
    assert "compile failed" in error.value.message


@final
class _FakeExecuteResult:
    def __init__(self, value: object = None) -> None:
        self.value = value

    def scalar_one_or_none(self) -> object:
        return self.value


@final
class _FakeAsyncSession:
    def __init__(self) -> None:
        self.added: list[object] = []
        self.committed: bool = False
        self.refreshed: object | None = None

    async def execute(self, _stmt: object) -> _FakeExecuteResult:
        return _FakeExecuteResult(None)

    def add(self, row: object) -> None:
        self.added.append(row)

    async def flush(self) -> None:
        if self.added:
            cast(_HasId, self.added[-1]).id = 7

    async def commit(self) -> None:
        self.committed = True

    async def refresh(self, row: object) -> None:
        self.refreshed = row


@pytest.mark.anyio
async def test_sqlalchemy_scl_import_repository_persists_source_model_and_diagnostics() -> None:
    session = _FakeAsyncSession()
    repository = Iec61850SqlAlchemySclImportRepository(cast(AsyncSession, cast(object, session)))
    record = Iec61850SclImportService(
        cast(Iec61850SclCompiler, cast(object, _RecordingCompiler(
            Iec61850SclCompilerOutput(
                schema=SCL_NORMALIZED_SCHEMA,
                selected_ied="IED1",
                source_size=len(b"<SCL/>"),
                model={"logicalDevices": [{"inst": "IED1LD0"}]},
                diagnostics=(Iec61850SclCompilerDiagnostic(severity="error", code="SCL_X", message="x"),),
            )
        ))),
        Iec61850InMemorySclImportRepository(),
    ).prepare_import_record(workspace_id=3, source=b"<SCL/>", filename="station.scd", selected_ied="IED1")

    saved = await repository.save(record, source=b"<SCL/>")

    assert session.committed is True
    assert len(session.added) == 1
    row = cast(_SavedImportRow, session.added[0])
    assert row.workspace_id == 3
    assert row.source_bytes == b"<SCL/>"
    assert _payload_rows(row.normalized_model, "logicalDevices")[0]["inst"] == "IED1LD0"
    assert row.diagnostics[0]["code"] == "SCL_X"
    assert saved.import_id == "7"
    assert saved.diagnostics[0].code == "SCL_X"


@final
class _FakeSelectionExecuteResult:
    def __init__(self, *, scalar: object = None, first: object = None) -> None:
        self._scalar = scalar
        self._first = first

    def scalar_one_or_none(self) -> object:
        return self._scalar

    def first(self) -> object:
        return self._first


@final
class _FakeSelectionSession:
    def __init__(self, import_row: object, selection_row: object = None) -> None:
        self.import_row = import_row
        self.selection_row = selection_row
        self.added: list[object] = []
        self.committed: bool = False
        self.refreshes: list[object] = []
        self.execute_count: int = 0

    async def execute(self, _stmt: object) -> _FakeSelectionExecuteResult:
        self.execute_count += 1
        if self.execute_count == 1:
            return _FakeSelectionExecuteResult(scalar=self.import_row)
        if self.execute_count == 2:
            return _FakeSelectionExecuteResult(scalar=self.selection_row)
        return _FakeSelectionExecuteResult(first=(self.selection_row, self.import_row) if self.selection_row else None)

    def add(self, row: object) -> None:
        self.added.append(row)
        if row.__class__.__name__ == "WorkspaceIec61850RuntimeSelection":
            cast(_HasId, row).id = 11
            self.selection_row = row
        elif row.__class__.__name__ == "WorkspaceIec61850RuntimeSelectionEvent":
            cast(_HasId, row).id = 12

    async def flush(self) -> None:
        return None

    async def commit(self) -> None:
        self.committed = True

    async def refresh(self, row: object) -> None:
        self.refreshes.append(row)


@final
class _ImportRow:
    id = 5
    workspace_id = 9
    source_filename = "station.scd"
    source_hash = "abc"
    source_size = 12
    selected_ied = "IED1"
    normalized_schema = SCL_NORMALIZED_SCHEMA
    normalized_model: dict[str, object] = {"reports": []}
    diagnostics: list[dict[str, object]] = []


@final
class _SelectionRow:
    id = 11
    workspace_id = 9
    scl_import_id = 4
    runtime_revision = 2
    selected_by = "old"
    selection_reason = "old"


@pytest.mark.anyio
async def test_runtime_selection_is_explicit_and_auditable() -> None:
    session = _FakeSelectionSession(_ImportRow())
    repository = Iec61850SqlAlchemySclImportRepository(cast(AsyncSession, cast(object, session)))

    selection = await repository.select_runtime_import(
        workspace_id=9,
        import_id="5",
        selected_by="operator",
        reason="commissioning",
    )

    assert selection.import_id == "5"
    assert selection.runtime_revision == 1
    assert selection.selected_ied == "IED1"
    assert session.committed is True
    assert [item.__class__.__name__ for item in session.added] == [
        "WorkspaceIec61850RuntimeSelection",
        "WorkspaceIec61850RuntimeSelectionEvent",
    ]
    event = cast(_SavedEvent, session.added[1])
    assert event.operation == "select"
    assert event.payload["sourceHash"] == "abc"
    assert event.payload["selectedIed"] == "IED1"


@pytest.mark.anyio
async def test_runtime_selection_revision_increments_only_when_import_changes() -> None:
    import_row = _ImportRow()
    selection_row = _SelectionRow()
    session = _FakeSelectionSession(import_row, selection_row)
    repository = Iec61850SqlAlchemySclImportRepository(cast(AsyncSession, cast(object, session)))

    selection = await repository.select_runtime_import(workspace_id=9, import_id="5", selected_by="operator")

    assert selection.runtime_revision == 3
    assert selection_row.scl_import_id == 5
    assert selection_row.runtime_revision == 3

    same_selection = _SelectionRow()
    same_selection.scl_import_id = 5
    same_session = _FakeSelectionSession(import_row, same_selection)
    same_repository = Iec61850SqlAlchemySclImportRepository(cast(AsyncSession, cast(object, same_session)))

    same = await same_repository.select_runtime_import(workspace_id=9, import_id="5", selected_by="operator")

    assert same.runtime_revision == 2
    assert same_selection.runtime_revision == 2
