#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

#include "taskhub/api_router.h"
#include "taskhub/http_server.h"
#include "taskhub/sqlite_task_repository.h"
#include "taskhub/task_service.h"

int main(int argc, char* argv[]) {
    try {
        const std::string database_path = argc > 1 ? argv[1] : "taskhub.db";
        const unsigned short port = argc > 2 ? static_cast<unsigned short>(std::stoi(argv[2])) : 8080;

        auto repository = std::make_shared<taskhub::SqliteTaskRepository>(database_path);
        auto service = std::make_shared<taskhub::TaskService>(repository);
        taskhub::ApiRouter router(service);
        taskhub::HttpServer server([&router](const taskhub::HttpRequest& request) {
            return router.handle(request);
        });
        server.listen(port);
    } catch (const std::exception& error) {
        std::cerr << "TaskHub failed to start: " << error.what() << std::endl;
        return EXIT_FAILURE;
    }
}
