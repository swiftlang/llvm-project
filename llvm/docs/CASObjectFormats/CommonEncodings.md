# Common encodings for object file concepts

There's some shared code for encodings for a few object file concepts.

Generic encoding functions (like VBR8) are in:

- llvm/include/llvm/CASObjectFormats/Encoding.h
- llvm/lib/CASObjectFormats/Encoding.cpp

Conceptual data are in:

- llvm/include/llvm/CASObjectFormats/Data.h
- llvm/lib/CASObjectFormats/Data.cpp

These are all pretty rough/WIP, and in some cases may have unnecessary
micro-optimizations that might inhibit fast algorithms using these.

## Encoding: VBR8

A bunch of numbers are encoded using "VBR8". This emits a sequence of
bytes, where the high bit of each byte (usually) indicates whether
there's more to read (only if there could be more data). Signed numbers have the sign rotated into the low-bit.

E.g.:

- `uint8_t` and `int8_t` use exactly 1B.
- `uint16_t` and `int16_t` use between 1B and 3B.
- `uint32_t` and `int32_t` use between 1B and 5B.
- `uint64_t` and `int64_t` use between 1B and 9B.

Note: we may want to stop using this if it's important to have fixed
offsets in more places.

## Data: FixupList

`data::Fixup` is a structure with a relocation kind and an offset.

A `data::FixupList` serializes an array of `Fixup`, and provides a lazy
view over such a serialization.

Encoding:
```
fixup-list := fixup*
fixup := fixup-kind fixup-offset
fixup-kind := uint8_t
fixup-offset := VBR8[int64_t]
```

Note that the size is not encoded. It must be stored or inferred from
external context. E.g.:
```
fixup-list-with-size := size fixup-list
size := VBR8[uint64_t]
```

## Data: TargetInfoList

`data::TargetInfo` is a structure with a target addend and a target
index.

A `data::TargetInfoList` serializes an array of `TargetInfo`, and
provides a lazy view over such a serialization.

Encoding:
```
target-info-list := target-info*
target-info := target-index-and-has-addend addend?

# target-index-and-has-addend: (index << 1) | bool(has-addend)
target-index-and-has-addend := VBR8[uint64_t]
target-addend := VBR8[int64_t]
```

Note that the size is not encoded. It must be stored or inferred from
external context.

## Data: BlockData

Encodes most data for a block. A few things that are missing should
probably be added eventually.

Encoding:
```
block-data := first-byte size content? fixup-list alignment-offset?
first-byte := alignment has-alignment-offset is-zerofill
# alignment: stored as lg2; up to 2^60
alignment := 6bits
has-alignment-offset := 1bit
is-zerofill := 1bit
size := VBR8[uint64_t]
# content: fixups zeroed out; skipped if is-zero-fill
content := uint8_t*
# alignment-offset: size depends on alignment
alignment-offset: uint8_t | uint16_t | uint32_t | uint64_t
```
The size of the fixup-list is inferred by walking back from the end
over the alignment-offset.
