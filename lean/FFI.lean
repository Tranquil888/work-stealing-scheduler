-- Runtime checker interface. The C++ side (src/verified.cpp) implements
-- these predicates over live snapshots; tests exercise them without a
-- Lean toolchain present.

-- @[extern "wss_no_task_loss"] opaque noTaskLossExtern : Bool
-- @[extern "wss_no_double_exec"] opaque noDoubleExecExtern : Bool
