# Proposal: `qWasmTableElement`, a packet that reads elements of a Wasm table

The goal of this proposal is to let a debugger get the address of the function
that an element of a Wasm table refers to.

WebAssembly code has no code addresses. Control flow is structured, with blocks,
loops and branches to labels. A direct call names a function by its index. An
indirect call (`call_indirect`) names an element of a table, and that element
refers to a function. A function pointer in compiled code is such a table index.

Debuggers still need addresses, for example to find the function, its symbol
and its debug information. The GDB remote protocol for Wasm gives each module
instance an address space (see "Wasm Packets"). But a table is run-time state
of the engine: a module can change its tables, and a table can hold functions
of other instances. Only the engine knows which function, and therefore which
address, a table element refers to at a given time.

## But why does the debugger need this

Two examples where this might be needed:

* In C++, the data formatters for a function pointer would need to convert the
table index into a function address.
* In Swift, when inspecting the async context of funclets, LLDB needs to
convert the continuation "pointer" from its index representation into a
function address.

## Proposed changes in `lldb/docs/resources/lldbgdbremote.md`

* A new packet, `qWasmTableElement`.
* A stop reply key, `wasm-table-version`, that tells a debugger when previous
`qWasmTableElement` information must be invalidated.
* A change to the `qWasmInstance` entry.

### New packet `qWasmTableElement`

Add this entry to the "Wasm Packets" section, after `qWasmStackValue`:

---

### qWasmTableElement

Get the functions that elements of Wasm tables refer to. A table index space
belongs to a module instance, so a table index only names a table together with
the module instance to read it from. Each request in the packet names a table,
an instance and an element:

```
send packet: $qWasmTableElement:elements:0,16,1;#e7
read packet: $0802000010000040#0f
```

This packet has one argument:

* `elements`: a list of triples of numbers, formatted in base-10. Each triple
is separated by a `,`, as is each number in each triple. Each triple is one
request: the table index, the instance id and the element index. The list has
at least one triple and must end with a `;`.

The format follows `MultiMemRead`, but in base 10, as in the other Wasm
packets. The instance id is the one that addresses carry, as in the
`;instance:<id>;` suffix of `qWasmInstance`. The table index is in the table
index space of that instance, which counts imported tables first.

The reply has one result per request, in request order, separated by commas. A
result is either the address of the function, as a 64-bit value hex-encoded in
little-endian byte order (16 hex digits, as in `qWasmCallStack`), or a
3-character error. The address is in the object space of the instance that
defines the function; for an imported function, this is not the instance that
the request names. Its offset is that of the function body: the first byte
after the body size in the code section, where the local declarations start.

A bad packet is answered with a single error:

```
send packet: $qWasmTableElement:elements:0,16,1,0,16;#d6
read packet: $E01#a6
```

In the example above, the second request has no element index.

If a single request can't be fulfilled, an error is returned in the
corresponding position; other requests still get their results:

```
send packet: $qWasmTableElement:elements:9999,16,1,0,16,2;#e8
read packet: $E03,1003000010000040#dd
```

| Error | Of | Meaning |
|---|---|---|
| `E01` | Packet | The packet is malformed, for example the count of values in `elements` is not a multiple of 3. |
| `E02` | Request | No live instance has that id. The stub must not answer with the table of another instance. |
| `E03` | Request | The instance has no table with that index. |
| `E04` | Request | The element index is out of the bounds of the table. |
| `E05` | Request | The element is a null reference. |
| `E06` | Request | The element is not a function with Wasm code, for example a host function or an `externref`. |

A stub that does not implement the packet sends an empty reply, as for any
unknown packet.

**Priority to Implement:** Only required for Wasm support. Necessary to find
the function that a table element refers to.

---

### Stop reply key `wasm-table-version`

Debuggers are allowed to assume that replies provided to `qWasmTableElement`
are valid until the stub invalidates the results. A stub that supports
`qWasmTableElement` must inform debuggers of invalidation by including a new
key in the stop reply packet: `wasm-table-version`.

Add this entry at the end of the "Lldb Extensions" list of the "Stop reply
packet extensions" section:

---

* `wasm-table-version` - `hex` -
  For Wasm targets only. An unsigned 64-bit counter.

  A stub that implements `qWasmTableElement`:
  * Sends the key in every `T` packet, including replies to `?` and
    `qThreadStopInfo`.
  * Increments the counter if, since its previous `T` packet, an element of a
    table of a live instance changed, a table grew, or a module instance was
    created or destroyed.

  Example of three stops:

  ```
  read packet: $T05thread:1;threads:1;thread-pcs:4000001000002377;00:7723000010000040;reason:breakpoint;wasm-table-version:0;#be
  send packet: $qWasmTableElement:elements:0,16,1;#e7
  read packet: $0802000010000040#0f
  send packet: $c#63
  read packet: $T05thread:1;threads:1;thread-pcs:4000001000002377;00:7723000010000040;reason:breakpoint;wasm-table-version:0;#be
  send packet: $c#63
  read packet: $T05thread:1;threads:1;thread-pcs:4000001000002377;00:7723000010000040;reason:breakpoint;wasm-table-version:1;#bf
  send packet: $qWasmTableElement:elements:0,16,1;#e7
  read packet: $0c03000010000040#3b
  ```

---

### Change to `qWasmInstance`

The entry says that a later query for instance-scoped state carries the
`;instance:<id>;` suffix, but `qWasmTableElement` names an instance in each
request. Replace the last paragraph before "Priority to Implement" with:

---

An instance is named with a `;instance:<id>;` suffix in place of a frame
index, in which the id is encoded as base 10. `qWasmGlobal` carries it,
because a global has no address to identify its instance. `qWasmTableElement`
instead names an instance, with the same id, in each request, so that one
packet can read the tables of several instances.

---
