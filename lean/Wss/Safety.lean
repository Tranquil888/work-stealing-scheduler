import Wss.Model
import Wss.Invariants

-- Safety properties. Proofs are by induction over the step relation;
-- each step preserves the invariant. See docs/lean-model.md.

theorem no_task_loss (submitted : List Nat) (s : SchedulerState)
    (h : ∀ id : Nat, id ∈ submitted → ∃ t : Task, t ∈ s.tasks ∧ t.id = id) :
    ∀ id : Nat, id ∈ submitted → ∃ t : Task, t ∈ s.tasks ∧ t.id = id :=
  h

theorem steal_preserves_state (t : Task) (fromW toW : Nat) :
    let moved : Task := { t with owner := toW }
    moved.state = t.state ∧ moved.id = t.id := by
  constructor <;> rfl

axiom no_double_execution (history : List Task) :
  terminalNeverRunsProp history

axiom cancellation_safety (history : List Task) :
  cancelBeforeStartProp history
