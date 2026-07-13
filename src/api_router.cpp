#include "taskhub/api_router.h"

#include <charconv>
#include <sstream>
#include <string_view>

#include <nlohmann/json.hpp>

#include "taskhub/errors.h"

namespace taskhub {
namespace {

using json = nlohmann::json;

json task_to_json(const Task& task) {
    return {
        {"id", task.id},
        {"title", task.title},
        {"description", task.description},
        {"status", to_string(task.status)},
        {"created_at", task.created_at},
        {"updated_at", task.updated_at},
    };
}

HttpResponse json_response(int status, json body) {
    return HttpResponse{status, body.dump()};
}

HttpResponse error_response(int status, const std::string& message) {
    return json_response(status, {{"error", {{"message", message}}}});
}

std::optional<std::string> optional_string(const json& body, const char* field) {
    if (!body.contains(field)) {
        return std::nullopt;
    }
    if (!body.at(field).is_string()) {
        throw ValidationError(std::string(field) + " must be a string");
    }
    return body.at(field).get<std::string>();
}

json parse_json_body(const std::string& body) {
    if (body.empty()) {
        throw ValidationError("request body must be valid JSON");
    }
    json parsed = json::parse(body);
    if (!parsed.is_object()) {
        throw ValidationError("request body must be a JSON object");
    }
    return parsed;
}

std::optional<long long> task_id_from_path(const std::string& path) {
    constexpr std::string_view prefix = "/tasks/";
    if (path.rfind(prefix.data(), 0) != 0 || path.size() == prefix.size()) {
        return std::nullopt;
    }
    const std::string_view id_text(path.data() + prefix.size(), path.size() - prefix.size());
    long long id = 0;
    const auto parsed = std::from_chars(id_text.data(), id_text.data() + id_text.size(), id);
    if (parsed.ec != std::errc{} || parsed.ptr != id_text.data() + id_text.size() || id <= 0) {
        return std::nullopt;
    }
    return id;
}

std::optional<std::string> query_value(const std::string& query, const std::string& key) {
    std::istringstream stream(query);
    std::string part;
    while (std::getline(stream, part, '&')) {
        const std::size_t separator = part.find('=');
        if (separator != std::string::npos && part.substr(0, separator) == key) {
            return part.substr(separator + 1);
        }
    }
    return std::nullopt;
}

}  // namespace

ApiRouter::ApiRouter(std::shared_ptr<TaskService> service) : service_(std::move(service)) {}

HttpResponse ApiRouter::handle(const HttpRequest& request) const {
    try {
        if (request.method == "GET" && request.path == "/health") {
            return json_response(200, {{"data", {{"status", "ok"}}}});
        }

        if (request.method == "POST" && request.path == "/tasks") {
            const json body = parse_json_body(request.body);
            const auto title = optional_string(body, "title");
            if (!title.has_value()) {
                throw ValidationError("title is required");
            }
            const Task task = service_->create(CreateTask{*title, optional_string(body, "description").value_or("")});
            return json_response(201, {{"data", task_to_json(task)}});
        }

        if (request.method == "GET" && request.path == "/tasks") {
            std::optional<TaskStatus> status;
            const auto status_value = query_value(request.query, "status");
            if (status_value.has_value()) {
                status = task_status_from_string(*status_value);
                if (!status.has_value()) {
                    throw ValidationError("status must be todo, doing, or done");
                }
            }
            json tasks = json::array();
            for (const Task& task : service_->list(status)) {
                tasks.push_back(task_to_json(task));
            }
            return json_response(200, {{"data", tasks}});
        }

        const auto id = task_id_from_path(request.path);
        if (id.has_value() && request.method == "GET") {
            const auto task = service_->find_by_id(*id);
            return task.has_value() ? json_response(200, {{"data", task_to_json(*task)}})
                                    : error_response(404, "task not found");
        }

        if (id.has_value() && request.method == "PATCH") {
            const json body = parse_json_body(request.body);
            UpdateTask update;
            update.title = optional_string(body, "title");
            update.description = optional_string(body, "description");
            const auto status = optional_string(body, "status");
            if (status.has_value()) {
                update.status = task_status_from_string(*status);
                if (!update.status.has_value()) {
                    throw ValidationError("status must be todo, doing, or done");
                }
            }
            const auto task = service_->update(*id, update);
            return task.has_value() ? json_response(200, {{"data", task_to_json(*task)}})
                                    : error_response(404, "task not found");
        }

        if (id.has_value() && request.method == "DELETE") {
            return service_->remove(*id) ? HttpResponse{204, ""} : error_response(404, "task not found");
        }

        return error_response(404, "route not found");
    } catch (const nlohmann::json::exception&) {
        return error_response(400, "request body must be valid JSON");
    } catch (const ValidationError& error) {
        return error_response(400, error.what());
    } catch (const RepositoryError&) {
        return error_response(500, "database operation failed");
    } catch (const std::exception&) {
        return error_response(500, "internal server error");
    }
}

}  // namespace taskhub
