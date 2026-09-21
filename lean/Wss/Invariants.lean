import Wss.Model

def idsUnique (s : SchedulerState) : Prop :=
  ∀ a b : Task, a ∈ s.tasks → b ∈ s.tasks → a.id = b.id → a = b

def wellFormed (s : SchedulerState) : Prop := idsUnique s

def terminalNeverRunsProp (history : List Task) : Prop :=
  ∀ i j : Nat, ∀ t : Task,
    i < j → history[i]? = some t →
    isTerminal t.state = true →
    ∀ u : Task, history[j]? = some u → u.id = t.id →
      u.state ≠ .running

def cancelBeforeStartProp (history : List Task) : Prop :=
  ∀ i j k : Nat, ∀ t u v : Task,
    i < j → j < k →
    history[i]? = some t → t.state = .pending →
    history[j]? = some u → u.id = t.id → u.state = .cancelled →
    history[k]? = some v → v.id = t.id →
      v.state ≠ .running
