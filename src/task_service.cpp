#include "taskhub/task_service.h"

#include <algorithm>
#include <cctype>
#include <utility>

#include "taskhub/errors.h"

namespace taskhub {
namespace {

bool is_blank(const std::string& value) {
    return std::all_of(value.begin(), value.end(), [](unsigned char character) {
        return std::isspace(character) != 0;
    });
}

}  // namespace

TaskService::TaskService(std::shared_ptr<TaskRepository> repository) : repository_(std::move(repository)) {
    if (repository_ == nullptr) {
        throw ValidationError("Task repository must not be null");
    }
}

void TaskService::validate_title(const std::string& title) {
    if (title.empty() || is_blank(title)) {
        throw ValidationError("title must not be empty");
    }
    if (title.size() > 200) {
        throw ValidationError("title must be 200 characters or fewer");
    }
}

void TaskService::validate_description(const std::string& description) {
    if (description.size() > 2000) {
        throw ValidationError("description must be 2000 characters or fewer");
    }
}

Task TaskService::create(CreateTask request) {
    validate_title(request.title);
    validate_description(request.description);
    return repository_->create(request);
}

std::optional<Task> TaskService::find_by_id(long long id) {
    if (id <= 0) {
        return std::nullopt;
    }
    return repository_->find_by_id(id);
}

std::vector<Task> TaskService::list(std::optional<TaskStatus> status) {
    return repository_->list(status);
}

std::optional<Task> TaskService::update(long long id, UpdateTask request) {
    if (id <= 0) {
        return std::nullopt;
    }
    if (!request.title.has_value() && !request.description.has_value() && !request.status.has_value()) {
        throw ValidationError("at least one updatable field is required");
    }
    if (request.title.has_value()) {
        validate_title(*request.title);
    }
    if (request.description.has_value()) {
        validate_description(*request.description);
    }
    return repository_->update(id, request);
}

bool TaskService::remove(long long id) {
    return id > 0 && repository_->remove(id);
}

}  // namespace taskhub
