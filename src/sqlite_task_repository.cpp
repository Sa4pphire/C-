#include "taskhub/sqlite_task_repository.h"

#include <memory>
#include <utility>

#include "taskhub/errors.h"

namespace taskhub {
namespace {

class Statement {
public:
    Statement(sqlite3* database, const char* sql) {
        const int result = sqlite3_prepare_v2(database, sql, -1, &statement_, nullptr);
        if (result != SQLITE_OK) {
            throw RepositoryError("Unable to prepare SQL statement: " +
                                  std::string(sqlite3_errmsg(database)));
        }
    }

    ~Statement() { sqlite3_finalize(statement_); }

    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    sqlite3_stmt* get() const { return statement_; }

private:
    sqlite3_stmt* statement_{nullptr};
};

void bind_text(sqlite3_stmt* statement, int index, const std::string& value) {
    if (sqlite3_bind_text(statement, index, value.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        throw RepositoryError("Unable to bind SQL text value");
    }
}

Task row_to_task(sqlite3_stmt* statement) {
    const auto text_at = [statement](int index) {
        const unsigned char* text = sqlite3_column_text(statement, index);
        return text == nullptr ? std::string{} : reinterpret_cast<const char*>(text);
    };

    const auto status = task_status_from_string(text_at(3));
    if (!status.has_value()) {
        throw RepositoryError("Database contains an invalid task status");
    }
    return Task{sqlite3_column_int64(statement, 0), text_at(1), text_at(2), *status, text_at(4),
                text_at(5)};
}

}  // namespace

SqliteTaskRepository::SqliteTaskRepository(const std::string& database_path) {
    const int result = sqlite3_open_v2(database_path.c_str(), &database_,
                                       SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
                                       nullptr);
    if (result != SQLITE_OK) {
        const std::string message = database_ == nullptr ? "Unknown SQLite error" : sqlite3_errmsg(database_);
        sqlite3_close(database_);
        database_ = nullptr;
        throw RepositoryError("Unable to open database: " + message);
    }
    sqlite3_busy_timeout(database_, 3000);
    migrate();
}

SqliteTaskRepository::~SqliteTaskRepository() {
    if (database_ != nullptr) {
        sqlite3_close(database_);
    }
}

void SqliteTaskRepository::throw_on_error(int result, const char* action) const {
    if (result != SQLITE_OK && result != SQLITE_DONE && result != SQLITE_ROW) {
        throw RepositoryError(std::string(action) + ": " + sqlite3_errmsg(database_));
    }
}

void SqliteTaskRepository::migrate() {
    std::lock_guard<std::mutex> lock(mutex_);
    const char* sql = R"sql(
        CREATE TABLE IF NOT EXISTS tasks (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            title TEXT NOT NULL,
            description TEXT NOT NULL DEFAULT '',
            status TEXT NOT NULL CHECK(status IN ('todo', 'doing', 'done')) DEFAULT 'todo',
            created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
        );
    )sql";
    char* error = nullptr;
    const int result = sqlite3_exec(database_, sql, nullptr, nullptr, &error);
    if (result != SQLITE_OK) {
        const std::string message = error == nullptr ? sqlite3_errmsg(database_) : error;
        sqlite3_free(error);
        throw RepositoryError("Unable to migrate database: " + message);
    }
}

Task SqliteTaskRepository::create(const CreateTask& request) {
    std::lock_guard<std::mutex> lock(mutex_);
    Statement statement(database_, "INSERT INTO tasks(title, description) VALUES(?, ?)");
    bind_text(statement.get(), 1, request.title);
    bind_text(statement.get(), 2, request.description);
    throw_on_error(sqlite3_step(statement.get()), "Unable to create task");
    return *find_by_id_unlocked(sqlite3_last_insert_rowid(database_));
}

std::optional<Task> SqliteTaskRepository::find_by_id_unlocked(long long id) {
    Statement statement(database_,
                        "SELECT id, title, description, status, created_at, updated_at "
                        "FROM tasks WHERE id = ?");
    if (sqlite3_bind_int64(statement.get(), 1, id) != SQLITE_OK) {
        throw RepositoryError("Unable to bind task id");
    }
    const int result = sqlite3_step(statement.get());
    if (result == SQLITE_DONE) {
        return std::nullopt;
    }
    throw_on_error(result, "Unable to find task");
    return row_to_task(statement.get());
}

std::optional<Task> SqliteTaskRepository::find_by_id(long long id) {
    std::lock_guard<std::mutex> lock(mutex_);
    return find_by_id_unlocked(id);
}

std::vector<Task> SqliteTaskRepository::list(std::optional<TaskStatus> status) {
    std::lock_guard<std::mutex> lock(mutex_);
    const char* all_tasks = "SELECT id, title, description, status, created_at, updated_at "
                            "FROM tasks ORDER BY id DESC";
    const char* filtered_tasks = "SELECT id, title, description, status, created_at, updated_at "
                                 "FROM tasks WHERE status = ? ORDER BY id DESC";
    Statement statement(database_, status.has_value() ? filtered_tasks : all_tasks);
    if (status.has_value()) {
        bind_text(statement.get(), 1, to_string(*status));
    }

    std::vector<Task> tasks;
    while (true) {
        const int result = sqlite3_step(statement.get());
        if (result == SQLITE_DONE) {
            break;
        }
        throw_on_error(result, "Unable to list tasks");
        tasks.push_back(row_to_task(statement.get()));
    }
    return tasks;
}

std::optional<Task> SqliteTaskRepository::update(long long id, const UpdateTask& request) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto existing = find_by_id_unlocked(id);
    if (!existing.has_value()) {
        return std::nullopt;
    }

    const std::string title = request.title.value_or(existing->title);
    const std::string description = request.description.value_or(existing->description);
    const TaskStatus status = request.status.value_or(existing->status);
    Statement statement(database_,
                        "UPDATE tasks SET title = ?, description = ?, status = ?, "
                        "updated_at = CURRENT_TIMESTAMP WHERE id = ?");
    bind_text(statement.get(), 1, title);
    bind_text(statement.get(), 2, description);
    bind_text(statement.get(), 3, to_string(status));
    if (sqlite3_bind_int64(statement.get(), 4, id) != SQLITE_OK) {
        throw RepositoryError("Unable to bind task id");
    }
    throw_on_error(sqlite3_step(statement.get()), "Unable to update task");
    return find_by_id_unlocked(id);
}

bool SqliteTaskRepository::remove(long long id) {
    std::lock_guard<std::mutex> lock(mutex_);
    Statement statement(database_, "DELETE FROM tasks WHERE id = ?");
    if (sqlite3_bind_int64(statement.get(), 1, id) != SQLITE_OK) {
        throw RepositoryError("Unable to bind task id");
    }
    throw_on_error(sqlite3_step(statement.get()), "Unable to delete task");
    return sqlite3_changes(database_) > 0;
}

}  // namespace taskhub
