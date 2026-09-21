# Design decisions

## Why interfaces where they are

- `IWorkQueue`: two real implementations (`MutexQueue`, `ChaseLevDeque`)
  with identical users. The worker loop is written once against the
  interface; benchmarks pick the implementation. Genuine strategy.
- `IWorkStealingPolicy`: victim selection is an independent axis from the
  queue. Random vs. round-robin is a one-line swap at construction and the
  main thing benchmarks compare. Genuine strategy.
- `ISchedulingPolicy`: FIFO vs. LIFO pop order for the mutex queue.
  Small, but it controls a real tradeoff (fairness vs. cache locality) and
  is injectable without touching the worker.
- `ITimerService` / `IExecutor` / `ITaskObserver`: seam for tests. Timers
  and metrics are verified against fakes without starting threads.
- `ITask`: type erasure. The queues and workers must hold heterogeneous
  tasks (`FunctionTask`, `FutureTask<T>` for every `T`) in one container.
  A template base would force the queue to be templated too; a virtual
  `execute()` keeps one queue type.

## Why not abstract

- `SharedState<T>`: concrete template. A mutex + variant + condition
  variable has one correct shape; virtual dispatch would only hide it.
- `Worker`: concrete owner of thread + queue. Nothing else will ever
  implement "worker"; test doubles are not needed because stealing is
  tested through `Scheduler` and the queue classes directly.
- No `MyMutex` wrappers. `std::mutex` is the abstraction.

## Global queue first, deques second

Phase 1 uses one `MutexQueue` shared by all workers. It is obviously
correct and obviously contended — the baseline every later benchmark must
beat. Work stealing only pays once the baseline numbers exist.

## Chase–Lev, from first principles

Owner pushes/pops at the bottom, thieves steal from the top, so the two
sides contend only when a single element remains. That case is settled by
one CAS on `top`. Ordering: release on push, acquire on pop/steal
observations, `seq_cst` on the single-element CAS. No verbatim copy from
any paper; the capacity is fixed (8192 slots) to keep the proof surface
small.

One protocol rule matters more than the rest: push and pop must be
single-owner. The first version let `Scheduler::route` push from any
submitter thread straight into the deque and lost tasks under contention —
the owner could rewind `bottom` over a concurrent push. The fix is a
per-worker mutex inbox: external threads post there, and only the owning
worker moves inbox tasks into its deque. Stealing still needs no locks.

## Futures without blocking workers

`Future<T>::then(f, scheduler)` never blocks a worker. The continuation is
registered on the shared state; when the predecessor completes, a small
closure is submitted to the scheduler which computes `f` and fulfils the
successor. Exceptions (including `TaskCancelled`) propagate into the
successor. A blocking `wait()` inside a worker would deadlock a saturated
pool; callbacks do not.

## Cooperative cancellation

Preemptive cancellation of running C++ code is unsafe, so cancellation is
a flag. The scheduler guarantees the cheap half: a task whose token is set
before it starts never runs (it transitions `Pending → Cancelled`). A
running task must poll `is_cancelled()` itself. `cancel()` on a handle only
succeeds while pending; running tasks are left alone.

## Heap timers, not a timer wheel

The plan called for a timer wheel. A wheel shines with thousands of
short-lived timers ticking at fixed resolution; this runtime has few timers
with arbitrary deadlines, where a min-heap gives O(log n) insert/cancel and
exact wakeups with far less code to prove wrong. The class is therefore
named `TimerService`, not `TimerWheel`. One dedicated thread sleeps until
the next deadline, then submits due work to the pool — worker threads never
block on time. Periodic timers schedule `last + k*interval`, not
`now + interval`, so they do not drift.

## Lean proves safety, not liveness

"No task is lost" and "no task runs twice" are state invariants checkable
per transition. Liveness ("every task eventually runs") needs fairness
assumptions about the OS scheduler that the model cannot discharge, so it
is tested (stress tests + `wait_idle`) rather than proved.
