# Lean model

The `lean/` directory is a self-contained Lean 4 (`lake`) project. It is
documentation-grade: it mirrors the C++ state machine closely enough that
each invariant below is both a Lean theorem and a runtime check in
`include/wss/verified.hpp` (tested by `tests/verified_test.cpp`).

## Model (`lean/Wss/Model.lean`)

- `TaskState = pending | running | completed | cancelled | failed`
- `Task = { id, state, owner }`
- `SchedulerState = { tasks : List Task }`
- `Step`: `submit | start | finishOk | finishErr | cancel | steal`

`steal` changes only `owner`, never `state`.

## Invariants (`lean/Wss/Invariants.lean`)

- `WellFormed`: ids are unique.
- `TerminalNeverRuns`: `completed | cancelled | failed` never steps to `running`.
- `CancelBeforeStart`: a task cancelled while pending never steps to `running`.

## Theorems (`lean/Wss/Safety.lean`)

1. **No task loss**: every submitted id is in exactly one of the five
   states after any sequence of steps.
2. **No double execution**: terminal states never return to `running`.
3. **Cancellation safety**: `cancel` before `start` implies the task never runs.
4. **Steal safety**: `steal` preserves state and the task multiset.

Liveness is out of scope (see `docs/design-decisions.md`).

## FFI (`lean/FFI.lean`)

Declares `@[extern]` stubs for a verified transition checker. The C++
counterpart is `src/verified.cpp`: it implements the same predicates over
live scheduler snapshots so tests exercise the proved properties without
requiring a Lean toolchain at build time.
