"""Domain-specific errors for execution and allocation validation."""
from __future__ import annotations


class DomainError(Exception):
    """Base class for domain errors that should be handled cleanly."""

class ChannelNotFoundError(DomainError):
    """Raised when a referenced channel does not exist in the database."""


class SequenceNotApplicableError(DomainError):
    """Raised when a sequence has no channel/signal references."""


class SequenceStepBlockedError(DomainError):
    """Raised when a step cannot run because its target device is unavailable."""

    def __init__(self, message: str, *, unit_id: str) -> None:
        super().__init__(message)
        self.unit_id = unit_id


class SequenceDeviceUnavailableError(DomainError):
    """Raised when a hardware command makes its target device unavailable."""

    def __init__(self, message: str, *, unit_id: str) -> None:
        super().__init__(message)
        self.unit_id = unit_id
