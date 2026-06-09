from __future__ import annotations

from app.api.v1.iec61850.router import router, scl_router


def test_iec61850_scl_import_router_keeps_client_paths_separate() -> None:
    client_paths = {route.path for route in router.routes}
    scl_paths = {route.path for route in scl_router.routes}

    assert "/api/v1/iec61850/client/state" in client_paths
    assert "/api/v1/workspaces/{workspace_id}/iec61850/scl/import" in scl_paths
