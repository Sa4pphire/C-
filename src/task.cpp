#include "taskhub/task.h"

namespace taskhub {

std::string to_string(TaskStatus status) {
    switch (status) {
        case TaskStatus::Todo:
            return "todo";
        case TaskStatus::Doing:
            return "doing";
        case TaskStatus::Done:
            return "done";
    }
    return "todo";
}

std::optional<TaskStatus> task_status_from_string(const std::string& value) {
    if (value == "todo") {
        return TaskStatus::Todo;
    }
    if (value == "doing") {
        return TaskStatus::Doing;
    }
    if (value == "done") {
        return TaskStatus::Done;
    }
    return std::nullopt;
}

}  // namespace taskhub
