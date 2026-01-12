"""Domain-specific errors for execution and allocation validation."""
from __future__ import annotations


class DomainError(Exception):
    """Base class for domain errors that should be handled cleanly."""


class AllocationInvalidError(DomainError):
    """Raised when allocation payload is empty or malformed."""


class ChannelNotAllocatedError(DomainError):
    """Raised when a referenced channel or signal is missing from allocation."""


class ChannelNotFoundError(DomainError):
    """Raised when a referenced channel does not exist in the database."""


class SequenceNotApplicableError(DomainError):
    """Raised when a sequence has no channel/signal references."""


class TestRunInvalidStateError(DomainError):
    """Raised when a test run cannot transition to the requested state."""
