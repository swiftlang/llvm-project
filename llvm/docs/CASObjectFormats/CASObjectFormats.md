# CAS Object (File) Formats

## Objects supported by the CAS

There are three types of objects in LLVMCAS:
- Blob: opaque sequence of bytes (`StringRef`).
- Tree: filesystem tree, mapping names to other CAS objects. Similar to a tree
  in Git, and not particularly relevant here.
- Node: generic node, referencing any number of CAS objects plus an opaque
  sequence of bytes.

The object format schemas mostly use `Node`s.

- The top-level `Node` reserves its first CAS reference for a "type-id". This
  is DAG of CAS objects that identifies the schema.
- Currently, the kind for other nodes is in their first byte of data. This is
  a convenience for debugging / dumping them.

## Schemas

See documentation for specifics of schemas:

- NestedV1.md (a nested CAS-based object format)
- FlatV1.md (a flat CAS-based object format)

There is also a separate document for some common encodings:

- CommonEncodings.md

### Split up descriptor sections

These formats/schemas are optimized for when mega-sections like `__eh_frame`
and `__compact_unwind` are split up into separate blocks/atoms, with anonymous
symbols created for each record.

### Split up string pools

Similarly, string sections should be split up with an anonymous symbol for each
string.

## Major caveats

There are major caveats that prevent the current formats from being useful in
from being useful in practice. These mostly stem from them handling
serialization to/from `jitlink::LinkGraph` and not much else. That said,
they're useful for experimenting with encoding schemes and demonstrating
storage optimization headroom.

Not clear whether we'll evolve one of these or create a new one.

### Caveat: Does not support real algorithms

At a minimum, we want the format/interface to support:

- Builder APIs in a compiler backend.
- APIs / data structures to support linking concepts, such as:
    - Symbol
    - Block
    - Atom
    - GOT
- Lazy-loading during linking.

Currently, these are not supported.

### Caveat: Missing high-level object file pieces

This just serializes what's in `jitlink::LinkGraph`. These lack high-level
object file things like Mach-O load commands.

### Caveat: Relies on various types in `jitlink` namespace

There are various types in `jitlink` (such as `Edge::Kind`) that these
formats use directly. This is bad for a few reasons:
- They shouldn't depend on `jitlink`
- `jitlink` types are all unstable (can change at any time)
- `jitlink` types only handle the things the JIT cares about. E.g.,
  `Edge::Kind` does not handle all relocation types, and in some cases
  fails to differentiate between them.
