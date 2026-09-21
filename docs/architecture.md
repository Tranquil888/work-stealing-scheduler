# Architecture

## Class hierarchy

```
ITask (abstract)
├── TaskBase (shared id/state/cancellation boilerplate)
│   ├── FunctionTask
│   ├── ContinuationTask
│   └── TimerTask
└── FutureTask<T> (declared in future.hpp, also derives TaskBase)

IExecutor (abstract)
└── Scheduler

IWorkQueue (abstract)
├── MutexQueue
└── ChaseLevDeque

IWorkStealingPolicy (abstract)
├── RandomVictimPolicy
└── RoundRobinVictimPolicy

ISchedulingPolicy (abstract)
├── FifoPolicy
└── LifoPolicy

ICancellationToken (abstract)
└── CancellationTokenImpl
CancellationToken (value handle over shared_ptr<ICancellationToken>)
CancellationSource (cancelling side)

ITimerService (abstract)
└── TimerService (min-heap + one thread; see design-decisions.md)

ITaskObserver (abstract)
├── MetricsCollector
└── LogObserver
```

`SharedState<T>` is intentionally concrete: it is a template holding a
`mutex` + `condition_variable` + `variant<monostate, T, exception_ptr>`.
There is exactly one correct implementation, so an interface would only
add indirection. The same argument covers `TaskHandle`, `Future<T>`,
`Promise<T>`, `TimerHandle` and `Worker`: value types or concrete owners,
not extension points.

## Ownership

- `Scheduler` owns `Worker` objects via `unique_ptr`.
- `Worker` owns its `IWorkQueue` via `unique_ptr`.
- `TaskHandle` is a `shared_ptr<ITask>` wrapper; the scheduler and the
  queues hold `shared_ptr`s while a task is pending/running, users hold
  handles as long as they need them.
- `Future<T>` / `Promise<T>` share a `shared_ptr<SharedState<T>>`.
- Cancellation state is a `shared_ptr<CancellationTokenImpl>` shared
  between `CancellationSource` (writer) and `CancellationToken` (reader).
- `TimerService` never owns the scheduler; it holds a `std::function`
  that submits work back. `TimerHandle` is a `weak_ptr`-based handle so a
  dead service fails closed instead of dangling.

## Threading model

Each `Worker` runs one thread in a loop:

1. Pop from the local queue.
2. Otherwise try stealing from peers via `IWorkStealingPolicy`.
3. Otherwise wait briefly on its own `condition_variable`.

`Scheduler::submit` places a task on a worker queue round-robin and wakes
that worker. Stealing needs no notification: thieves poll. Shutdown sets a
shared stop flag and notifies all workers; with `drain == true` workers
finish what is queued, otherwise pending tasks are cancelled in place.

## Task lifecycle

```
Pending → Running → Completed
              │   ├→ Failed (exception escaped)
              └→ Cancelled (cancel() while pending, or token
                            already set before start)
```

Cancelled is monotonic: once cancelled a task never runs, and a terminal
state never transitions again. This is the property the Lean model proves
and `verified.hpp` re-checks at runtime in tests.
