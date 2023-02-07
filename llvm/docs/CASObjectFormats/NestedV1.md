# NestedV1: a nested CAS-based object format

"NestedV1" is a CAS-optimized object format with a nested hierarchy.

The top-level CAS object is called a *compile-unit*. Most object file concepts
are split into separate CAS objects that reference each other in a fairly
complex graph. Some symbols are referenced directly as a CAS object reference;
others are referenced indirectly by their name (sometimes only to break a
cycle).

The code is in:

- llvm/include/llvm/CASObjectFormats/NestedV1.h
- llvm/lib/CASObjectFormats/NestedV1.cpp

See also: CommonEncodings.md.

## Major caveats with current state

This format currently:

- implicitly dead strips some things during creation
- makes a bit of a mess of symbol attributes

See also "Major caveats" section in CASObjectFormats.md.

## Structure

The top-level CAS object is called *compile-unit*, described last.

Two types called "variants": this is a CAS object reference where the type
needs to be detected by loading the CAS object and inspecting it.

- **name**: a name; often used as a reference
- **block**: an section of code that is treated as a single unit, including
  edges
    - **section**: *name* and attributes
    - **block-data**: data for a block, excluding targets
        - Includes raw content, alignment, and fixups (but not targets)
    - **encoded-data**: node kind used for big `data::TargetInfoList`s (see
      CommonEncodings.md). This array is parallel to the fixups in *block-data*
      and associates them with an index into *target-list* and an optional
      addend. When it's small, it's embedded directly in the *block*'s data.
    - **target-list**: a sorted list of *target*s
        - **target**: a variant of:
            - *symbol*: (see below)
            - *name*: points at the name of the symbol
- **symbol**: *name*, attributes, and a definition; a handle for an address
    - **symbol-definition**: a variant of:
        - *block*: regular symbol
        - *symbol*: alias that points directly at the aliased symbol
        - *name*: alias that points at the name of the symbol it aliases
- **symbol-table**: a sorted list of *symbol*s
- **compile-unit**: top-level object file; stores high-level data like
  target-triple directly; has three *symbol-table*s that are entry points to
  the symbol/block graph, but many symbols are not listed at the top level
    - DeadStripNever: *symbol-table* listing any symbol that is not
      allowed to be dead-stripped (even by the linker).
    - DeadStripLink: *symbol-table* listing any symbol that the linker can
      dead-strip if unreferenced, but that was not discardable for the compiler
      (e.g., the compiler can discard symbols for functions marked `inline` or
      `static` if they are not referenced; it generally must emit other
      functions)
    - IndirectDeadStripCompile: *symbol-table* listing any symbol that was
      discardable by the compiler and that is ever referenced by its *name*

Undefined/external symbols are referenced by *name* and not mentioned
elsewhere.

## Builder algorithm from `jitlink::LinkGraph`

The builder requires a `jitlink::LinkGraph` as input. The algorithm for
generating a *compile-unit* is:

- Sort symbols:
    - by scope, putting locals last
    - by linkage, putting strong symbols before weak symbols
    - by liveness, putting no-dead-strip symbols ahead of others
- For each symbol, do a post-order traversal of not-yet-visited symbols
  reachable from it.
    - Create the *symbol-definition*, if it doesn't yet exist. Since `jitlink`
      does not support first-class aliases, this is always a *block* right now.
        - When creating *target*s for the block, if the targeted *symbol*
          already exists it's used; otherwise, it's referenced by *name* (if it
          doesn't have one, a temporary name is generated on the fly, although
          the visitation order means this doesn't happen in practice).
    - Create the *symbol*.

### Implicit dead-stripping

Any subgraph of symbols/blocks that is not reachable from a symbol that the
compiler was required to emit is implicitly dead-stripped. It will not be
listed in any of the top-level *symbol-table*s.

Probably this is a bug, not a feature.

### Breaking cycles from `jitlink::Edge::KeepAlive`

Usually, a cycle is broken by referencing one of the symbols involved
in the cycle by *name*.

Any cycle involving a `jitlink::Edge::KeepAlive` edge is handled specially.

Such a cycle comes from a descriptor record. E.g., the FDE in the EH frame
section points at the function it describes, and a `KeepAlive` edge is added to
point back at the FDE.

To break this cycle, the FDE's target is serialized to point at an
"abstract-backedge" symbol. This representation of an FDE makes it equivalent
with most others (they de-dup when the target is punched out), and it serves as
a sort of "template" for constructing a block. At deserialization time, the
"abstract-backedge" is filled in to point at the symbol through which the
template was reached.

This works because the symbol visitation order always sees functions before
their FDE (if reached from another symbol, the function dominates its FDE).
