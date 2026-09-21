#include <algorithm>
#include <unordered_set>
#include <wss/verified.hpp>

namespace wss {

bool check_no_task_loss(const SchedulerSnapshot& snap,
                        const std::vector<std::uint64_t>& submitted) {
    if (snap.tasks.size() != submitted.size()) return false;
    std::unordered_set<std::uint64_t> seen;
    for (auto& t : snap.tasks) {
        if (!seen.insert(t.id).second) return false;
    }
    for (auto id : submitted) {
        if (seen.find(id) == seen.end()) return false;
    }
    return true;
}

bool check_no_double_execution(const std::vector<TaskRecord>& history) {
    std::unordered_set<std::uint64_t> terminal;
    for (auto& r : history) {
        if (terminal.find(r.id) != terminal.end()) {
            if (r.state == TaskState::Running) return false;
        }
        if (r.state == TaskState::Completed || r.state == TaskState::Cancelled ||
            r.state == TaskState::Failed) {
            terminal.insert(r.id);
        }
    }
    return true;
}

bool check_steal_preserves_state(TaskState before, TaskState after) {
    return before == after;
}

bool check_cancel_before_start_never_runs(
    const std::vector<TaskRecord>& history) {
    std::unordered_set<std::uint64_t> cancelled_while_pending;
    for (auto& r : history) {
        if (r.state == TaskState::Cancelled) cancelled_while_pending.insert(r.id);
        if (r.state == TaskState::Running &&
            cancelled_while_pending.find(r.id) != cancelled_while_pending.end()) {
            return false;
        }
    }
    return true;
}

}  // namespace wss
