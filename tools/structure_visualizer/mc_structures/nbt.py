"""Minimal NBT parser for Minecraft .nbt structure files.

The .nbt format used for structure files is gzip-compressed binary NBT.
This parser supports just enough to read structure files:
  - TAG_Compound, TAG_List, TAG_Int_Array, TAG_Byte_Array
  - TAG_Byte, TAG_Short, TAG_Int, TAG_Long, TAG_Float, TAG_Double, TAG_String
  - TAG_Long_Array (used by heightmaps etc., not by structures but parsed anyway)

Reference: https://minecraft.wiki/NBT_format
"""

from __future__ import annotations

import gzip
import struct
from dataclasses import dataclass
from typing import Any


# NBT tag IDs
TAG_END = 0
TAG_BYTE = 1
TAG_SHORT = 2
TAG_INT = 3
TAG_LONG = 4
TAG_FLOAT = 5
TAG_DOUBLE = 6
TAG_BYTE_ARRAY = 7
TAG_STRING = 8
TAG_LIST = 9
TAG_COMPOUND = 10
TAG_INT_ARRAY = 11
TAG_LONG_ARRAY = 12


@dataclass
class NBTList:
    """A list of NBT values of the same tag type."""
    tag_type: int
    elements: list


@dataclass
class NBTCompound:
    """A dict-like NBT compound. Keys are strings."""
    data: dict

    def get(self, name: str) -> Any | None:
        return self.data.get(name)

    def getList(self, name: str) -> NBTList | None:
        v = self.data.get(name)
        return v if isinstance(v, NBTList) else None

    def getCompound(self, name: str) -> "NBTCompound | None":
        v = self.data.get(name)
        return v if isinstance(v, NBTCompound) else None

    def getInt(self, name: str) -> int | None:
        v = self.data.get(name)
        return v if isinstance(v, int) else None

    def getString(self, name: str) -> str | None:
        v = self.data.get(name)
        return v if isinstance(v, str) else None


class NBTReader:
    def __init__(self, data: bytes):
        self.data = data
        self.pos = 0

    def read(self, n: int) -> bytes:
        if self.pos + n > len(self.data):
            raise EOFError(f"Read past end of NBT: need {n} at pos {self.pos}, "
                           f"only {len(self.data) - self.pos} left.")
        b = self.data[self.pos:self.pos + n]
        self.pos += n
        return b

    def read_byte(self) -> int:
        return self.read(1)[0]

    def read_short(self) -> int:
        return struct.unpack(">h", self.read(2))[0]

    def read_ushort(self) -> int:
        return struct.unpack(">H", self.read(2))[0]

    def read_int(self) -> int:
        return struct.unpack(">i", self.read(4))[0]

    def read_long(self) -> int:
        return struct.unpack(">q", self.read(8))[0]

    def read_float(self) -> float:
        return struct.unpack(">f", self.read(4))[0]

    def read_double(self) -> float:
        return struct.unpack(">d", self.read(8))[0]

    def read_string(self) -> str:
        length = self.read_ushort()
        return self.read(length).decode("utf-8")

    def read_payload(self, tag_type: int) -> Any:
        if tag_type == TAG_BYTE:
            return struct.unpack(">b", self.read(1))[0]
        if tag_type == TAG_SHORT:
            return self.read_short()
        if tag_type == TAG_INT:
            return self.read_int()
        if tag_type == TAG_LONG:
            return self.read_long()
        if tag_type == TAG_FLOAT:
            return self.read_float()
        if tag_type == TAG_DOUBLE:
            return self.read_double()
        if tag_type == TAG_BYTE_ARRAY:
            length = self.read_int()
            return list(self.read(length))
        if tag_type == TAG_STRING:
            return self.read_string()
        if tag_type == TAG_LIST:
            elem_type = self.read_byte()
            length = self.read_int()
            elements = []
            for _ in range(length):
                elements.append(self.read_payload(elem_type))
            return NBTList(elem_type, elements)
        if tag_type == TAG_COMPOUND:
            data = {}
            while True:
                t = self.read_byte()
                if t == TAG_END:
                    break
                name = self.read_string()
                value = self.read_payload(t)
                data[name] = value
            return NBTCompound(data)
        if tag_type == TAG_INT_ARRAY:
            length = self.read_int()
            return list(struct.unpack(f">{length}i", self.read(4 * length)))
        if tag_type == TAG_LONG_ARRAY:
            length = self.read_int()
            return list(struct.unpack(f">{length}q", self.read(8 * length)))
        raise ValueError(f"Unknown NBT tag type: {tag_type}")

    def read_root(self) -> NBTCompound:
        t = self.read_byte()
        if t != TAG_COMPOUND:
            raise ValueError(f"Expected root TAG_Compound, got tag {t}")
        name = self.read_string()
        return self.read_payload(t)


def read_nbt_gzip(path: str) -> NBTCompound:
    """Read a gzip-compressed .nbt file and return the root compound."""
    with gzip.open(path, "rb") as f:
        data = f.read()
    return NBTReader(data).read_root()
