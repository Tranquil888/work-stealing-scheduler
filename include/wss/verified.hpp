#pragma once

#include <cstdint>
#include <vector>

#include <wss/task.hpp>

namespace wss {

struct TaskRecord {
    std::uint64_t id{0};
    TaskState state{TaskState::Pending};
    std::size_t owner{0};
};

struct SchedulerSnapshot {
    std::vector<TaskRecord> tasks;
};

bool check_no_task_loss(const SchedulerSnapshot& snap,
                        const std::vector<std::uint64_t>& submitted);

bool check_no_double_execution(const std::vector<TaskRecord>& history);

bool check_steal_preserves_state(TaskState before, TaskState after);

bool check_cancel_before_start_never_runs(const std::vector<TaskRecord>& history);

}  // namespace wss
