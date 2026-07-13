#pragma once

#include <memory>

#include "taskhub/http_server.h"
#include "taskhub/task_service.h"

namespace taskhub {

class ApiRouter {
public:
    explicit ApiRouter(std::shared_ptr<TaskService> service);
    HttpResponse handle(const HttpRequest& request) const;

private:
    std::shared_ptr<TaskService> service_;
};

}  // namespace taskhub
