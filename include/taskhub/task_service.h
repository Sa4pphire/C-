#pragma once

#include <memory>
#include <optional>
#include <vector>

#include "taskhub/task_repository.h"

namespace taskhub {

class TaskService {
public:
    explicit TaskService(std::shared_ptr<TaskRepository> repository);

    Task create(CreateTask request);
    std::optional<Task> find_by_id(long long id);
    std::vector<Task> list(std::optional<TaskStatus> status);
    std::optional<Task> update(long long id, UpdateTask request);
    bool remove(long long id);

private:
    static void validate_title(const std::string& title);
    static void validate_description(const std::string& description);
    std::shared_ptr<TaskRepository> repository_;
};

}  // namespace taskhub
