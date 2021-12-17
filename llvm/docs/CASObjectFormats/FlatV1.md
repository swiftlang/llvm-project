# FlatV1: a "flat"/simple CAS-optimized object format

"FlatV1" is a CAS-optimized object format with a flat hierarchy.

The top-level CAS object is called a *compile-unit*. Descriptions of
symbols/blocks/atoms are outlined into leaf CAS objects and the graph is stored
in data at the top level.

This format is called "flat" because there is no nesting; the only CAS object
that directly references other objects is compile-unit.

The code is in:

- llvm/include/llvm/CASObjectFormats/FlatV1.h
- llvm/lib/CASObjectFormats/FlatV1.cpp

See also: CommonEncodings.md.

## Major caveats with current state

This format has a monolithic top-level node that describes most things. This
node never repeats when an object file changes, so whats inside it cannot be
deduped. This format is smaller than opaque Mach-O and it has much better
growth rate, but its growth rate is not as good as NestedV1.

For example, we've looked at data for storing 100 builds of consecutive commits
of LLVM. The storage cost overhead for 100 builds vs. 1 build is:

- release builds
    - Opaque: 56%
    - FlatV1: 23%
    - NestedV1: 13%
- no-opt builds (`-O0 -gnone`)
    - Opaque: 74%
    - FlatV1: 28%
    - NestedV1: 11%

See also "Major caveats" section in CASObjectFormats.md.

## Structure

The top-level CAS object is called *compile-unit*. This top-level object is a
serialized traversal of all the sections, symbols, and blocks, defining each in
terms of outlined templates.

**compile-unit**: all graph information stored at the top-level

- Data: all symbols, blocks, sections, referring to templates with a 4B index
- templates[]: an array of description templates, each one of:
    - **section**: name; attributes
    - **symbol**: name; attributes
    - **block**: binary content; fixups; target addends

There are `llvm-cas-object-format` command-line options that can outline more
templates but the others haven't been profitable.

## Builder algorithm

The builder walks the graph doing two things in parallel:

- Construct an array of templates, using a uniquing pool to avoid duplicates
  (effectively, a `SetVector` from LLVMADT).
- Construct a byte stream for *compile-unit*'s top-level data that fully
  describes the graph, using 4B references into the templates where
  appropriate.

The graph is walked as follows:

- For each section
    - Encode section (and remember ordering)
- Define ordering for symbols and blocks
    - Sort symbols:
        - by scope, putting locals last
        - by linkage, putting strong symbols before weak symbols
        - by liveness, putting no-dead-strip symbols ahead of others
    - Sort blocks:
        - by section
        - by address
- For each symbol:
    - Encode symbol:
        - Reference symbol template by index into templates
        - Reference block by index into creation order
- For each block:
    - Encode block (including edges):
        - Reference block template by index into templates
        - Reference section by index into creation order
        - Reference symbols by index into creation order

