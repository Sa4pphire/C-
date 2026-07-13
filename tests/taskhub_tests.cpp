#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

#include "taskhub/api_router.h"
#include "taskhub/errors.h"
#include "taskhub/sqlite_task_repository.h"
#include "taskhub/task_service.h"

namespace {

using json = nlohmann::json;
using taskhub::ApiRouter;
using taskhub::CreateTask;
using taskhub::HttpRequest;
using taskhub::SqliteTaskRepository;
using taskhub::Task;
using taskhub::TaskService;
using taskhub::TaskStatus;
using taskhub::UpdateTask;

class TestFailure : public std::runtime_error {
public:
    explicit TestFailure(const std::string& message) : std::runtime_error(message) {}
};

#define EXPECT_TRUE(condition)                                                                    \
    do {                                                                                          \
        if (!(condition)) {                                                                       \
            throw TestFailure(std::string("Expectation failed: ") + #condition);                \
        }                                                                                         \
    } while (false)

#define EXPECT_EQ(actual, expected) EXPECT_TRUE((actual) == (expected))

struct Context {
    std::shared_ptr<SqliteTaskRepository> repository = std::make_shared<SqliteTaskRepository>(":memory:");
    std::shared_ptr<TaskService> service = std::make_shared<TaskService>(repository);
    ApiRouter router{service};
};

taskhub::HttpResponse request(Context& context, const std::string& method, const std::string& path,
                              const std::string& body = "") {
    return context.router.handle(HttpRequest{method, path, "", {}, body});
}

taskhub::HttpResponse request_with_query(Context& context, const std::string& path, const std::string& query) {
    return context.router.handle(HttpRequest{"GET", path, query, {}, ""});
}

template <typename Function>
void expect_validation_error(Function function) {
    try {
        function();
    } catch (const taskhub::ValidationError&) {
        return;
    }
    throw TestFailure("Expected ValidationError");
}

void test_create_has_default_status() {
    Context context;
    const Task task = context.service->create({"Study RAII", "Read and practice"});
    EXPECT_TRUE(task.id > 0);
    EXPECT_EQ(task.status, TaskStatus::Todo);
    EXPECT_EQ(task.title, "Study RAII");
}

void test_create_keeps_description() {
    Context context;
    const Task task = context.service->create({"HTTP", "Build a server"});
    EXPECT_EQ(task.description, "Build a server");
}

void test_find_returns_created_task() {
    Context context;
    const Task created = context.service->create({"STL", ""});
    const auto found = context.service->find_by_id(created.id);
    EXPECT_TRUE(found.has_value());
    EXPECT_EQ(found->title, "STL");
}

void test_find_missing_returns_empty() {
    Context context;
    EXPECT_TRUE(!context.service->find_by_id(999).has_value());
}

void test_list_returns_newest_first() {
    Context context;
    const Task first = context.service->create({"First", ""});
    const Task second = context.service->create({"Second", ""});
    const auto tasks = context.service->list(std::nullopt);
    EXPECT_EQ(tasks.size(), 2U);
    EXPECT_EQ(tasks.front().id, second.id);
    EXPECT_TRUE(tasks.back().id == first.id);
}

void test_update_title() {
    Context context;
    const Task task = context.service->create({"Old", ""});
    UpdateTask update;
    update.title = "New";
    const auto updated = context.service->update(task.id, update);
    EXPECT_TRUE(updated.has_value());
    EXPECT_EQ(updated->title, "New");
}

void test_update_description() {
    Context context;
    const Task task = context.service->create({"Task", "Old description"});
    UpdateTask update;
    update.description = "New description";
    const auto updated = context.service->update(task.id, update);
    EXPECT_EQ(updated->description, "New description");
}

void test_update_status_and_filter() {
    Context context;
    const Task task = context.service->create({"Task", ""});
    UpdateTask update;
    update.status = TaskStatus::Doing;
    context.service->update(task.id, update);
    const auto tasks = context.service->list(TaskStatus::Doing);
    EXPECT_EQ(tasks.size(), 1U);
    EXPECT_EQ(tasks[0].id, task.id);
}

void test_remove_existing_task() {
    Context context;
    const Task task = context.service->create({"Delete", ""});
    EXPECT_TRUE(context.service->remove(task.id));
    EXPECT_TRUE(!context.service->find_by_id(task.id).has_value());
}

void test_remove_missing_task() {
    Context context;
    EXPECT_TRUE(!context.service->remove(100));
}

void test_blank_title_is_rejected() {
    Context context;
    expect_validation_error([&context] { context.service->create({"   ", ""}); });
}

void test_long_title_is_rejected() {
    Context context;
    expect_validation_error([&context] { context.service->create({std::string(201, 'x'), ""}); });
}

void test_long_description_is_rejected() {
    Context context;
    expect_validation_error(
        [&context] { context.service->create({"Task", std::string(2001, 'x')}); });
}

void test_empty_update_is_rejected() {
    Context context;
    const Task task = context.service->create({"Task", ""});
    expect_validation_error([&context, id = task.id] { context.service->update(id, {}); });
}

void test_health_endpoint() {
    Context context;
    const auto response = request(context, "GET", "/health");
    EXPECT_EQ(response.status, 200);
    EXPECT_EQ(json::parse(response.body)["data"]["status"].get<std::string>(), "ok");
}

void test_create_endpoint() {
    Context context;
    const auto response = request(context, "POST", "/tasks", R"({"title":"Build API","description":"SQLite"})");
    EXPECT_EQ(response.status, 201);
    EXPECT_EQ(json::parse(response.body)["data"]["title"].get<std::string>(), "Build API");
}

void test_create_endpoint_requires_title() {
    Context context;
    const auto response = request(context, "POST", "/tasks", R"({"description":"only"})");
    EXPECT_EQ(response.status, 400);
}

void test_invalid_json_returns_bad_request() {
    Context context;
    const auto response = request(context, "POST", "/tasks", "not json");
    EXPECT_EQ(response.status, 400);
}

void test_list_endpoint_filters_status() {
    Context context;
    const Task task = context.service->create({"Filter", ""});
    UpdateTask update;
    update.status = TaskStatus::Done;
    context.service->update(task.id, update);
    const auto response = request_with_query(context, "/tasks", "status=done");
    EXPECT_EQ(response.status, 200);
    EXPECT_EQ(json::parse(response.body)["data"].size(), 1U);
}

void test_get_missing_endpoint_returns_not_found() {
    Context context;
    const auto response = request(context, "GET", "/tasks/404");
    EXPECT_EQ(response.status, 404);
}

void test_patch_invalid_status_returns_bad_request() {
    Context context;
    const Task task = context.service->create({"Status", ""});
    const auto response = request(context, "PATCH", "/tasks/" + std::to_string(task.id),
                                  R"({"status":"later"})");
    EXPECT_EQ(response.status, 400);
}

void test_delete_endpoint_returns_no_content() {
    Context context;
    const Task task = context.service->create({"Delete endpoint", ""});
    const auto response = request(context, "DELETE", "/tasks/" + std::to_string(task.id));
    EXPECT_EQ(response.status, 204);
}

void test_concurrent_creates_preserve_all_tasks() {
    Context context;
    constexpr int worker_count = 16;
    std::vector<std::thread> workers;
    for (int i = 0; i < worker_count; ++i) {
        workers.emplace_back([&context, i] { context.service->create({"Concurrent " + std::to_string(i), ""}); });
    }
    for (auto& worker : workers) {
        worker.join();
    }
    EXPECT_EQ(context.service->list(std::nullopt).size(), static_cast<std::size_t>(worker_count));
}

}  // namespace

int main() {
    const std::vector<std::pair<std::string, std::function<void()>>> tests = {
        {"create has default status", test_create_has_default_status},
        {"create keeps description", test_create_keeps_description},
        {"find returns created task", test_find_returns_created_task},
        {"find missing", test_find_missing_returns_empty},
        {"list is newest first", test_list_returns_newest_first},
        {"update title", test_update_title},
        {"update description", test_update_description},
        {"update status and filter", test_update_status_and_filter},
        {"remove existing task", test_remove_existing_task},
        {"remove missing task", test_remove_missing_task},
        {"blank title", test_blank_title_is_rejected},
        {"long title", test_long_title_is_rejected},
        {"long description", test_long_description_is_rejected},
        {"empty update", test_empty_update_is_rejected},
        {"health endpoint", test_health_endpoint},
        {"create endpoint", test_create_endpoint},
        {"missing title endpoint", test_create_endpoint_requires_title},
        {"invalid json", test_invalid_json_returns_bad_request},
        {"list endpoint filter", test_list_endpoint_filters_status},
        {"missing endpoint", test_get_missing_endpoint_returns_not_found},
        {"invalid patch status", test_patch_invalid_status_returns_bad_request},
        {"delete endpoint", test_delete_endpoint_returns_no_content},
        {"concurrent creates", test_concurrent_creates_preserve_all_tasks},
    };

    int failed = 0;
    for (const auto& [name, test] : tests) {
        try {
            test();
            std::cout << "[PASS] " << name << '\n';
        } catch (const std::exception& error) {
            ++failed;
            std::cerr << "[FAIL] " << name << ": " << error.what() << '\n';
        }
    }
    std::cout << tests.size() - failed << "/" << tests.size() << " tests passed\n";
    return failed == 0 ? 0 : 1;
}
