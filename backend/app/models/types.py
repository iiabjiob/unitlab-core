"""Shared SQLAlchemy column helpers."""
from __future__ import annotations

from sqlalchemy import BigInteger, Integer

#pragma to ensure compatibility with sqlite fallback
BIGINT_PK = BigInteger().with_variant(Integer, "sqlite")
