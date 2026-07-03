from __future__ import annotations

from typing import Any
import re

from app.services.iec61850.mms_adapter import (
    Iec61850MmsEndpointCatalog,
    Iec61850MmsEndpointCatalogEntry,
    build_mms_endpoint_catalog,
)
from app.services.verification_planner import VerificationTargetSource
from app.schemas.verification_schema import VerificationSubscriptionPlanSchema

_IP_PATTERN = re.compile(r"(?<!\d)(?:\d{1,3}\.){3}\d{1,3}(?!\d)")
_HOST_KEY_TOKENS = (
    "transport_host",
    "mms_host",
    "endpoint_host",
    "ied_host",
    "server_host",
    "host",
    "ip_address",
    "device_ip",
    "target_ip",
    "server_ip",
    "ip",
)


def build_verification_signal_endpoint_catalog(
    *,
    sources: list[VerificationTargetSource],
    subscription_plan: VerificationSubscriptionPlanSchema,
) -> Iec61850MmsEndpointCatalog | None:
    entries_by_key: dict[tuple[str, str, str], Iec61850MmsEndpointCatalogEntry] = {}
    for group in subscription_plan.groups:
        if not group.access_point_name or group.access_point_name.lower() == "unknown":
            continue
        group_sources = [
            sources[target_index]
            for target_index in group.target_indexes
            if 0 <= target_index < len(sources)
        ]
        endpoint = _extract_target_endpoint_candidate(group_sources)
        if endpoint is None:
            continue
        host, port = endpoint
        ied_name = group.ied_name.strip() if group.ied_name else ""
        endpoint_id = group.endpoint_id.strip() if group.endpoint_id else None
        key = (ied_name.lower(), group.access_point_name.strip().lower(), endpoint_id or "")
        if key in entries_by_key and entries_by_key[key].host == host and entries_by_key[key].port == port:
            continue
        if key in entries_by_key and (entries_by_key[key].host != host or entries_by_key[key].port != port):
            continue
        entries_by_key[key] = Iec61850MmsEndpointCatalogEntry(
            ied_name=ied_name,
            access_point_name=group.access_point_name,
            host=host,
            port=port,
            endpoint_id=endpoint_id,
        )
    if not entries_by_key:
        return None
    return build_mms_endpoint_catalog(list(entries_by_key.values()))


def build_verification_plan_endpoint_catalog(
    subscription_plan: VerificationSubscriptionPlanSchema,
) -> Iec61850MmsEndpointCatalog | None:
    entries_by_key: dict[tuple[str, str, str], Iec61850MmsEndpointCatalogEntry] = {}
    for group in subscription_plan.groups:
        if not group.access_point_name or group.access_point_name.lower() == "unknown":
            continue
        target_metadata = [
            subscription_plan.targets[target_index].protocol_metadata
            for target_index in group.target_indexes
            if 0 <= target_index < len(subscription_plan.targets)
        ]
        endpoint = _extract_endpoint_candidate(target_metadata)
        if endpoint is None:
            continue
        host, port = endpoint
        ied_name = group.ied_name.strip() if group.ied_name else ""
        endpoint_id = group.endpoint_id.strip() if group.endpoint_id else None
        key = (ied_name.lower(), group.access_point_name.strip().lower(), endpoint_id or "")
        existing = entries_by_key.get(key)
        if existing is not None and (existing.host != host or existing.port != port):
            continue
        entries_by_key[key] = Iec61850MmsEndpointCatalogEntry(
            ied_name=ied_name,
            access_point_name=group.access_point_name,
            host=host,
            port=port,
            endpoint_id=endpoint_id,
        )
    if not entries_by_key:
        return None
    return build_mms_endpoint_catalog(list(entries_by_key.values()))


def _extract_target_endpoint_candidate(sources: list[VerificationTargetSource]) -> tuple[str, int] | None:
    metadata_items = [
        getattr(source, "signal_metadata", None)
        for source in sources
        if isinstance(getattr(source, "signal_metadata", None), dict)
    ]
    return _extract_endpoint_candidate(metadata_items)


def _extract_target_host_candidates(sources: list[VerificationTargetSource]) -> list[str]:
    prioritized: list[str] = []
    fallback: list[str] = []
    for source in sources:
        metadata = getattr(source, "signal_metadata", None)
        if not isinstance(metadata, dict) or not metadata:
            continue
        keys = _extract_by_key_priority(metadata, _HOST_KEY_TOKENS)
        for candidate in keys:
            normalized = _normalize_ipv4_candidate(candidate)
            if normalized and normalized not in prioritized:
                prioritized.append(normalized)
        if not prioritized:
            for candidate in _extract_ipv4_strings(metadata):
                if candidate not in fallback:
                    fallback.append(candidate)
    return prioritized or fallback


def _extract_endpoint_candidate(metadata_items: list[dict[str, Any]]) -> tuple[str, int] | None:
    for metadata in metadata_items:
        for candidate in _extract_by_key_priority(metadata, _HOST_KEY_TOKENS):
            endpoint = _normalize_host_port_candidate(candidate)
            if endpoint is not None:
                return endpoint
    for metadata in metadata_items:
        for candidate in _extract_ipv4_strings(metadata):
            endpoint = _normalize_host_port_candidate(candidate)
            if endpoint is not None:
                return endpoint
    return None


def _extract_by_key_priority(payload: Any, key_tokens: tuple[str, ...]) -> list[str]:
    values: list[str] = []
    if isinstance(payload, dict):
        for key, child in payload.items():
            key_text = str(key).strip().lower()
            if any(token == key_text or token in key_text for token in key_tokens):
                values.extend(_extract_strings(child))
            values.extend(_extract_by_key_priority(child, key_tokens))
    elif isinstance(payload, (list, tuple, set)):
        for item in payload:
            values.extend(_extract_by_key_priority(item, key_tokens))
    return values


def _extract_strings(value: Any) -> list[str]:
    if isinstance(value, str):
        return [value]
    if isinstance(value, (list, tuple, set)):
        result: list[str] = []
        for item in value:
            result.extend(_extract_strings(item))
        return result
    if isinstance(value, dict):
        result: list[str] = []
        for child in value.values():
            result.extend(_extract_strings(child))
        return result
    return []


def _extract_ipv4_strings(value: Any) -> list[str]:
    candidates: list[str] = []
    for raw in _extract_strings(value):
        for match in _IP_PATTERN.findall(raw):
            normalized = _normalize_ipv4_candidate(match)
            if normalized and normalized not in candidates:
                candidates.append(normalized)
    return candidates


def _normalize_ipv4_candidate(value: str) -> str | None:
    text = str(value).strip()
    if not text:
        return None
    try:
        import ipaddress

        return str(ipaddress.ip_address(text))
    except ValueError:
        return None


def _normalize_host_port_candidate(value: str) -> tuple[str, int] | None:
    text = str(value).strip()
    if not text:
        return None
    first_token = text.split(None, 1)[0].strip()
    host_text = first_token or text
    port = 102

    bracket_match = re.fullmatch(r"\[([^\]]+)\](?::(\d{1,5}))?", host_text)
    if bracket_match:
        host_text = bracket_match.group(1)
        if bracket_match.group(2):
            port = int(bracket_match.group(2))
    else:
        host_part, separator, port_part = host_text.rpartition(":")
        if separator and host_part and port_part.isdigit() and ":" not in host_part:
            host_text = host_part
            port = int(port_part)

    host_text = host_text.strip()
    if not host_text or port <= 0 or port > 65535:
        return None
    return host_text, port
