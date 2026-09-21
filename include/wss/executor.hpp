#pragma once

#include <wss/task_handle.hpp>

namespace wss {

class IExecutor {
public:
    virtual ~IExecutor() = default;
    virtual TaskHandle submit_task(TaskPtr task) = 0;
};

}  // namespace wss
