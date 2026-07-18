# End-to-End Test Cases

Run the suite with `just test` (builds the profiler first), or directly:

```sh
python3 tests/run.py                 # everything
python3 tests/run.py trio lambda     # substring filters
python3 tests/run.py --list          # enumerate cases
python3 tests/run.py -v <case>       # show the derived attribution table
python3 tests/run.py --build-dir <dir> --clangxx <path>   # non-default toolchain
```

Requires Python >= 3.11, no third-party packages. Per-case artifacts (binary,
build log, `malloc_stats.json`) are kept in `tests/.work/<case>/`.

Each case is a directory under `tests/cases/` containing:

- one or more `*.cpp` files (all are compiled and linked into a single test
  binary, built with the plugin + hook prelude)
- `expect.toml` — semantic expectations checked against the `malloc_stats.json`
  the profiler emits when the binary runs under the runtime library.

Every case is _also_ checked against the universal invariants in
`tests/harness/checks.py` (cross-reference validity, sorted events, sane object
records, etc.) — those need no per-case configuration.

Levels are documentation, not directory structure: **L1** = basic attribution
for plain user types, **L2** = exotic-type coverage, **L3** = member/base
attribution and offset correctness. L2 and L3 are orthogonal axes; several cases
carry both tags.

Design rules applied throughout:

- **Distinctive sizes.** Every allocation in a case uses a size unique within
  that case, so misattribution breaks an exact assertion instead of passing
  silently.
- **Negative controls.** Cases with two or more instrumented types assert each
  type's bytes exactly; cross-attribution therefore fails two assertions. Fields
  that allocate nothing assert `bytes = 0`.
- **Fixed-width members.** L3 structs use `int64_t`/`int32_t`/pointer-sized
  members only, so field offsets are identical on every 64-bit Itanium-ABI
  platform and can be asserted exactly with no platform conditionals.
- **Known issues are xfail, not silence.** A case expected to fail on a known
  GitHub issue is marked `xfail = "gh-N"` (optionally platform-qualified) in its
  `expect.toml`. An xfail that _passes_ is reported prominently so a fix flips
  the case to a permanent regression guard.

| Case                     | Level | What it validates                                                                                                                                                                                                 |
| ------------------------ | ----- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `basic_new_delete`       | L1    | Two plain structs (`Foo`, `Bar`) with user-written inline dtors, `new[]`/`delete[]` of distinct sizes. Baseline: typed attribution works at all; per-type exact bytes + object counts (mutual negative controls). |
| `basic_malloc_free`      | L1    | Same shape but the C allocation path: `malloc` in ctor, `free` in dtor. On macOS this guards the `__DATA,__interpose` table (plain symbol export cannot interpose the C allocator there).                         |
| `aligned_new`            | L2    | Over-aligned type via `new`/`delete` (the `align_val_t` operator pair). `sizeof != alignof` by design, so swapped memalign arguments would corrupt the heap.                                                      |
| `aligned_c_api`          | L1    | `posix_memalign` + `aligned_alloc` owned/freed by a dtor. Interposed on macOS via the dyld table; hooked under their own exported names on Linux (glibc's versions bypass the `memalign` hook).                   |
| `object_counts`          | L1    | One type destroyed 5 times (3 stack, 2 heap) with per-instance allocations; asserts `objects = 5` (distinct destructor invocations) and total bytes.                                                              |
| `out_of_line_dtor`       | L1    | Dtor declared in-class, defined out-of-line in the same TU (external linkage → eagerly emitted by CodeGen; regression guard for gh-2).                                                                            |
| `split_tu_dtor`          | L1    | Dtor defined in a _second_ translation unit (two `.cpp` files); same eager-emission mechanism as `out_of_line_dtor` (gh-2 guard).                                                                                 |
| `out_of_line_defaulted`  | L1    | Out-of-line `= default` dtor: external linkage _and_ defaulted, so it must be rewritten eagerly but only once Sema has synthesized its body.                                                                      |
| `explicit_instantiation` | L2    | Explicit instantiation definition (weak_odr) + implicit instantiation (linkonce_odr) of one template; both deferred-emission paths must be instrumented.                                                          |
| `nested_ownership`       | L1    | `Outer` contains `Inner` by value; `Inner` owns heap memory. Pins down the attribution-precedence contract: bytes appear under both (transitively), and under `Outer`'s `inner` field.                            |
| `global_object`          | L1    | Global with an owning member, destroyed during exit. Guards global variable tracking (formerly gh-1, fixed by the mp_unwind final-frame fix).                                                                     |
| `template_owner`         | L2    | User template `Holder<T>` instantiated at `int` and `double` with distinct sizes; per-instantiation type names and exact bytes.                                                                                   |
| `lambda_capture`         | L2    | Lambda with three init-captures (vectors of distinct sizes). Lambda type matched by pattern (`(lambda at *`), fields are unnamed → matched by offset.                                                             |
| `std_vector_trivial`     | L2    | `std::vector<int>` with exact reserve: container instrumentation, buffer bytes exact.                                                                                                                             |
| `std_vector_owning`      | L2    | `std::vector<Elem>` where each `Elem` owns heap memory; element bytes chain through the vector (`reserve` keeps buffer size exact).                                                                               |
| `std_array_owning`       | L2/L3 | `std::array<vec,3>` of owning elements (implicit dtor of a template): array type owns the elements' bytes through its single c-array field.                                                                       |
| `variant_alternatives`   | L2    | `std::variant` over two owning alternatives, one object of each; variant owns both totals.                                                                                                                        |
| `string_heap`            | L2    | Heap `std::string` (large) next to SSO strings (no allocation). Exercises gh-3 (extern-template dtors bind into the C++ runtime library; all platforms).                                                          |
| `unique_ptr_owner`       | L2    | `std::unique_ptr<Owner>`: smart-pointer dtor chain owns both the `Owner` block and `Owner`'s own heap memory.                                                                                                     |
| `inheritance_single`     | L2/L3 | `Derived : Base`, both own memory. Base appears in `Derived`'s base table at offset 0; bytes attributed to each type exactly.                                                                                     |
| `inheritance_multiple`   | L2/L3 | `D : A, B` with fixed-width members: `B`'s base offset is nonzero and asserted exactly; attribution through each base separately.                                                                                 |
| `member_offsets`         | L3    | Struct with interleaved scalar and owning members; asserts exact field offsets/sizes from the type metadata table _and_ exact per-field byte attribution.                                                         |
| `same_type_members`      | L3    | `Trio { vec a, b, c }` — three members of the _same_ type, distinguishable only by size/offset. The sharpest cross-attribution detector.                                                                          |
| `c_array_member`         | L3    | `array3 { vec elems[3] }` (README's headline case): attribution through a c-array field, exact offsets.                                                                                                           |
| `empty_member_zero`      | L3    | Owning member next to a member that never allocates: the empty one asserts `bytes = 0` (absence assertion).                                                                                                       |

## IR-level checks

A case may opt in to IR verification with an `[ir]` table in its `expect.toml`.
Each source file is then compiled a second time with `-S -emit-llvm` and the
harness asserts that every emitted base/complete destructor definition contains
the injected payload (its `__MP_TYPE_DATA` function-local static) **exactly
once** — zero means a rewrite failed to land before CodeGen emitted the body
(the gh-2 failure mode, invisible to both the plugin's log and `-ast-dump`),
more than one means the payload was double-injected.
`instrumented = ["ClassName", ...]` additionally requires that a destructor
definition for each listed class was emitted at all, so the check cannot pass
vacuously. `available_externally` definitions are exempt (they mirror code
compiled into another image, e.g. extern-template members of libc++).

## Attribution semantics used by the checker

For each `FREE` event, the runtime records the chain of instrumented destructor
frames on the stack, innermost first. The checker derives:

- **`bytes` per type** — the freed size is added to every type appearing in the
  chain (deduplicated), i.e. "bytes freed while this type's destructor was
  live".
- **`objects` per type** — number of distinct destructor invocations
  (`object_id`s) of that type observed across all chains. Note: an object none
  of whose members allocate never appears in a chain and is not counted.
- **field/base attribution** — for each adjacent pair (child, parent) in a
  chain, `child.addr - parent.addr` is located within the parent's field table
  (or base table); the freed bytes are credited to that field/base of the parent
  type.
- **`untyped` bytes** — freed sizes whose chain is empty. Note that a heap
  object deleted _outside_ any instrumented destructor contributes its own block
  here by design (only its members' frees are typed).
