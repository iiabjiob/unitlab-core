from app.services.core_diag_event_forwarder import _incident_id


def _snapshot(*, cpu: float = 86.0, memory: float = 96.0, disk: float = 96.0) -> dict:
    return {
        "mode": "degraded",
        "cpu": {"temperature_c": cpu},
        "memory": {"used_percent": memory},
        "disk_root": {"used_percent": disk},
        "services": [{"name": "docker", "active": True}],
    }


def test_incident_id_ignores_numeric_measurement_drift() -> None:
    assert _incident_id(_snapshot(cpu=86.0, memory=96.0, disk=96.0)) == _incident_id(
        _snapshot(cpu=91.5, memory=98.0, disk=99.0)
    )


def test_incident_id_changes_for_different_required_service_failure() -> None:
    docker_down = _snapshot()
    docker_down["services"] = [{"name": "docker", "active": False}]
    network_manager_down = _snapshot()
    network_manager_down["services"] = [{"name": "NetworkManager", "active": False}]

    assert _incident_id(docker_down) != _incident_id(network_manager_down)


def test_healthy_snapshot_has_no_incident_id() -> None:
    assert _incident_id(
        {
            "mode": "ok",
            "cpu": {"temperature_c": 40.0},
            "memory": {"used_percent": 30.0},
            "disk_root": {"used_percent": 40.0},
            "services": [{"name": "docker", "active": True}],
        }
    ) is None
