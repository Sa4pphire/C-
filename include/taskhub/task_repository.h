#pragma once

#include <optional>
#include <vector>

#include "taskhub/task.h"

namespace taskhub {

class TaskRepository {
public:
    virtual ~TaskRepository() = default;
    virtual Task create(const CreateTask& request) = 0;
    virtual std::optional<Task> find_by_id(long long id) = 0;
    virtual std::vector<Task> list(std::optional<TaskStatus> status) = 0;
    virtual std::optional<Task> update(long long id, const UpdateTask& request) = 0;
    virtual bool remove(long long id) = 0;
};

}  // namespace taskhub
