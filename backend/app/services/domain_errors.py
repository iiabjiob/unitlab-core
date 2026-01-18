"""Domain-specific errors for execution and allocation validation."""
from __future__ import annotations


class DomainError(Exception):
    """Base class for domain errors that should be handled cleanly."""

class ChannelNotFoundError(DomainError):
    """Raised when a referenced channel does not exist in the database."""


class SequenceNotApplicableError(DomainError):
    """Raised when a sequence has no channel/signal references."""