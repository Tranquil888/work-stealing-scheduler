-- Task and scheduler state model. Mirrors include/wss/task.hpp.

inductive TaskState where
  | pending
  | running
  | completed
  | cancelled
  | failed
  deriving DecidableEq, Repr

structure Task where
  id : Nat
  state : TaskState
  owner : Nat
  deriving Repr

structure SchedulerState where
  tasks : List Task
  deriving Repr

inductive Step where
  | submit (t : Task)
  | start (id : Nat)
  | finishOk (id : Nat)
  | finishErr (id : Nat)
  | cancel (id : Nat)
  | steal (id fromWorker toWorker : Nat)
  deriving Repr

def isTerminal : TaskState → Bool
  | .completed => true
  | .cancelled => true
  | .failed => true
  | _ => false
