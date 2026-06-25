"""Structures subpackage. One file per structure family."""
from .fossils import (
    fossil_generator,
    fossil_reference,
    fossil_overworld,
    fossil_overworld_coal,
    fossil_nether,
    OVERWORLD_FOSSILS,
    NETHER_FOSSILS,
    FOSSIL_REGISTRY,
)

__all__ = [
    "fossil_generator",
    "fossil_reference",
    "fossil_overworld",
    "fossil_overworld_coal",
    "fossil_nether",
    "OVERWORLD_FOSSILS",
    "NETHER_FOSSILS",
    "FOSSIL_REGISTRY",
]
