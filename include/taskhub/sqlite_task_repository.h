#pragma once

#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <sqlite3.h>

#include "taskhub/task_repository.h"

namespace taskhub {

class SqliteTaskRepository final : public TaskRepository {
public:
    explicit SqliteTaskRepository(const std::string& database_path);
    ~SqliteTaskRepository() override;

    SqliteTaskRepository(const SqliteTaskRepository&) = delete;
    SqliteTaskRepository& operator=(const SqliteTaskRepository&) = delete;

    Task create(const CreateTask& request) override;
    std::optional<Task> find_by_id(long long id) override;
    std::vector<Task> list(std::optional<TaskStatus> status) override;
    std::optional<Task> update(long long id, const UpdateTask& request) override;
    bool remove(long long id) override;

private:
    void migrate();
    std::optional<Task> find_by_id_unlocked(long long id);
    void throw_on_error(int result, const char* action) const;

    sqlite3* database_{nullptr};
    std::mutex mutex_;
};

}  // namespace taskhub
