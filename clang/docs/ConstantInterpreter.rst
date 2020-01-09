====================
Constant Interpreter
====================

.. contents::
   :local:

Introduction
============

The constexpr interpreter aims to replace the existing tree evaluator in clang, improving performance on constructs which are executed inefficiently by the evaluator. The interpreter is activated using the following flags:

* ``-fexperimental-new-constant-interpreter`` enables the interpreter, emitting an error if an unsupported feature is encountered

Bytecode Compilation
====================

Bytecode compilation is handled in ``ByteCodeStmtGen.h`` for statements and ``ByteCodeExprGen.h`` for expressions. The compiler has two different backends: one to generate bytecode for functions (``ByteCodeEmitter``) and one to directly evaluate expressions as they are compiled, without generating bytecode (``EvalEmitter``). All functions are compiled to bytecode, while toplevel expressions used in constant contexts are directly evaluated since the bytecode would never be reused. This mechanism aims to pave the way towards replacing the evaluator, improving its performance on functions and loops, while being just as fast on single-use toplevel expressions.

The interpreter relies on stack-based, strongly-typed opcodes. The glue logic between the code generator, along with the enumeration and description of opcodes, can be found in ``Opcodes.td``. The opcodes are implemented as generic template methods in ``Interp.h`` and instantiated with the relevant primitive types by the interpreter loop or by the evaluating emitter.

Primitive Types
---------------

* ``PT_{U|S}int{8|16|32|64}``

  Signed or unsigned integers of a specific bit width, implemented using the ```Integral``` type.

* ``PT_{U|S}intFP``

  Signed or unsigned integers of an arbitrary, but fixed width used to implement
  integral types which are required by the target, but are not supported by the host.
  Under the hood, they rely on APValue. The ``Integral`` specialisation for these
  types is required by opcodes to share an implementation with fixed integrals.

* ``PT_Bool``

  Representation for boolean types, essentially a 1-bit unsigned ``Integral``.

* ``PT_RealFP``

  Arbitrary, but fixed precision floating point numbers. Could be specialised in
  the future similarly to integers in order to improve floating point performance.

* ``PT_Ptr``

  Pointer type, defined in ``"Pointer.h"``. A pointer can be either null, reference interpreter-allocated memory (``BlockPointer``) or point to an address which can be derived, but not accessed (``ExternPointer``).

* ``PT_FnPtr``

  Function pointer type, can also be a null function pointer. Defined in ``"FnPointer.h"``.

* ``PT_MemPtr``

  Member pointer type, can also be a null member pointer. Defined in ``"MemberPointer.h"``

* ``PT_VoidPtr``

  Void pointer type, can be used for rount-trip casts. Represented as the union of all pointers which can be cast to void. Defined in ``"VoidPointer.h"``.

* ``PT_ObjCBlockPtr``

  Pointer type for ObjC blocks. Defined in ``"ObjCBlockPointer.h"``.

Composite types
---------------

The interpreter distinguishes two kinds of composite types: arrays and records. Unions are represented as records, except at most a single field can be marked as active. The contents of inactive fields are kept until they are reactivated and overwritten. Complex numbers (``_Complex``) and vectors (``__attribute((vector_size(16)))``) are treated as arrays.


Bytecode Execution
==================

Bytecode is executed using a stack-based interpreter. The execution context consists of an ``InterpStack``, along with a chain of ``InterpFrame`` objects storing the call frames. Frames are built by call instructions and destroyed by return instructions. They perform one allocation to reserve space for all locals in a single block. These objects store all the required information to emit stack traces whenever evaluation fails.

Memory Organisation
===================

Memory management in the interpreter relies on 3 data structures: ``Block``
object which store the data and associated inline metadata, ``Pointer`` objects
which refer to or into blocks, and ``Descriptor`` structures which describe
blocks and subobjects nested inside blocks.

Blocks
------

Blocks contain data interleaved with metadata. They are allocated either statically
in the code generator (globals, static members, dummy parameter values etc.) or
dynamically in the interpreter, when creating the frame containing the local variables
of a function. Blocks are associated with a descriptor that characterises the entire
allocation, along with a few additional attributes:

* ``IsStatic`` indicates whether the block has static duration in the interpreter, i.e. it is not a local in a frame.

* ``DeclID`` identifies each global declaration (it is set to an invalid and irrelevant value for locals) in order to prevent illegal writes and reads involving globals and temporaries with static storage duration.

Static blocks are never deallocated, but local ones might be deallocated even when there are live pointers to them. Pointers are only valid as long as the blocks they point to are valid, so a block with pointers to it whose lifetime ends is kept alive until all pointers to it go out of scope. Since the frame is destroyed on function exit, such blocks are turned into a ``DeadBlock`` and copied to storage managed by the interpreter itself, not the frame. Reads and writes to these blocks are illegal and cause an appropriate diagnostic to be emitted. When the last pointer goes out of scope, dead blocks are also deallocated.

The lifetime of blocks is managed through 3 methods stored in the descriptor of the block:

* **CtorFn**: initializes the metadata which is store in the block, alongside actual data. Invokes the default constructors of objects which are not trivial (``Pointer``, ``RealFP``, etc.)
* **DtorFn**: invokes the destructors of non-trivial objects.
* **MoveFn**: moves a block to dead storage.

Non-static blocks track all the pointers into them through an intrusive doubly-linked list, required to adjust all pointers when transforming a block into a dead block.

Descriptors
-----------

Descriptor are generated at bytecode compilation time and contain information required to determine if a particular memory access is allowed in constexpr. Even though there is a single descriptor object, it encodes information for several kinds of objects:

* **Primitives**

  A block containing a primitive reserved storage only for the primitive.

* **Arrays of primitives**

  An array of primitives contains a pointer to an ``InitMap`` storage as its first field: the initialisation map is a bit map indicating all elements of the array which were initialised. If the pointer is null, no elements were initialised, while a value of ``(InitMap*)-1`` indicates that the object was fully initialised. when all fields are initialised, the map is deallocated and replaced with that token.

  Array elements are stored sequentially, without padding, after the pointer to the map.

* **Arrays of composites and records**

  Each element in an array of composites is preceded by an ``InlineDescriptor`` which stores the attributes specific to the field and not the whole allocation site. Descriptors and elements are stored sequentially in the block. Records are laid out identically to arrays of composites: each field and base class is preceded by an inline descriptor. The ``InlineDescriptor`` has the following field:

 * **Offset**: byte offset into the array or record, used to step back to the parent array or record.
 * **IsConst**: flag indicating if the field is const-qualified.
 * **IsInitialized**: flag indicating whether the field or element was initialized. For non-primitive fields, this is only relevant for base classes.
 * **IsBase**: flag indicating whether the record is a base class. In that case, the offset can be used to identify the derived class.
 * **IsActive**: indicates if the field is the active field of a union.
 * **IsMutable**: indicates if the field is marked as mutable.

Inline descriptors are filled in by the `CtorFn` of blocks, which leaves storage in an uninitialised, but valid state.

Pointers
--------

Pointers are represented as a union of ``BlockPointer``, ``ExternPointer`` and ``NullPointer``. ``BlockPointer`` is used to reference memory allocated and managed by the interpreter, being the only pointer kind which supports dereferencing. Null pointers cannot be dereferenced, but pointer arithmetic is allowed in them in a non-constexpr context, turning them into ``ExternPointer``. Extern pointers are almost identical to ``APValue``, tracking a declaration and a path of fields and indices into that allocation. All pointers are convertible to ``APValue``.

BlockPointer
~~~~~~~~~~~~

Block pointers track a ``Pointee``, the block to which they point, along with a ``Base`` and an ``Offset``. The base identifies the innermost field, while the offset points to an array element relative to the base (including one-past-end pointers). The offset identifies the array element or field which is referenced, while the base points to the outer object or array which contains the field. These two fields allow all pointers to be uniquely identified, disambiguated and characterised.

As an example, consider the following structure:

.. code-block:: c

    struct A {
        struct B {
            int x;
            int y;
        } b;
        struct C {
            int a;
            int b;
        } c[2];
        int z;
    };
    constexpr A a;

On the target, ``&a`` and ``&a.b.x`` are equal. So are ``&a.c[0]`` and ``&a.c[0].a``. In the interpreter, all these pointers must be distinguished since the are all allowed to address distinct range of memory.

In the interpreter, the object would require 240 bytes of storage and would have its field interleaved with metadata. The pointers which can be derived to the object are illustrated in the following diagram:

::

      0   16  32  40  56  64  80  96  112 120 136 144 160 176 184 200 208 224 240
  +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
  + B | D | D | x | D | y | D | D | D | a | D | b | D | D | a | D | b | D | z |
  +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
      ^   ^   ^       ^       ^   ^   ^       ^       ^   ^       ^       ^
      |   |   |       |       |   |   |   &a.c[0].b   |   |   &a.c[1].b   |
      a   |&a.b.x   &a.y    &a.c  |&a.c[0].a          |&a.c[1].a          |
        &a.b                   &a.c[0]            &a.c[1]               &a.z

The ``Base`` offset of all pointers points to the start of a field or an array and is preceded by an inline descriptor (unless ``Base`` is zero, pointing to the root). All the relevant attributes can be read from either the inline descriptor or the descriptor of the block.


Array elements are identified by the ``Offset`` field of pointers, pointing to past the inline descriptors for composites and before the actual data in the case of primitive arrays. The ``Offset`` points to the offset where primitives can be read from. As an example, ``a.c + 1`` would have the same base as ``a.c`` since it is an element of ``a.c``, but its offset would point to ``&a.c[1]``. The array-to-pointer decay operation adjusts a pointer to an array (where the offset is equal to the base) to a pointer to the first element.

ExternPointer
~~~~~~~~~~~~~

Extern pointers can be derived, pointing into symbols which are not readable from constexpr. An external pointer consists of a base declaration, along with a path designating a subobject, similar to the ``LValuePath`` of an APValue. Extern pointers can be converted to block pointers if the underlying variable is defined after the pointer is created, as is the case in the following example:

.. code-block:: c

  extern const int a;
  constexpr const int *p = &a;
  const int a = 5;
  static_assert(*p == 5, "x");

Null Pointers
~~~~~~~~~~~~~

While null pointer arithmetic is banned in constexpr, some expressions on null constants must be folded, replicating the behavious of the ``offsetof`` builtin. Null pointers are characterised by two offsets: a field offset and an array offset, along with a descriptor specifying the type the pointer is supposed to refer to. Array indexing ajusts the array offset, while the field offset is adjusted when a pointer to a member is created.

TODO
====

Missing Language Features
-------------------------

* Changing the active field of unions
* Pointer-Integer-Pointer round trips
* ``typeid``
* ``volatile``
* ``mutable``
* ``__builtin_constant_p``
* ``dynamic_cast``
* ``new`` and ``delete``
* Complex and vector arithmetic
* GCC statement expressions

Performance Improvements
------------------------

* Initialisation of very large arrays: currently instructions are emitted for
  each element of ``ArrayInitLoopExpr``, a for loop should be generated instead
* Zero-initialisation still sets most elements to 0 through individual
  instructions, this should be achieved in the ``Descriptor``'s ``InitFn``

Known Bugs
----------

* If execution fails, memory storing APInts and APFloats is leaked when the stack is cleared
