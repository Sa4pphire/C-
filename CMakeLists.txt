cmake_minimum_required(VERSION 3.20)
project(TaskHub VERSION 0.1.0 LANGUAGES CXX)

if(NOT UNIX)
    message(FATAL_ERROR "TaskHub requires a POSIX-compatible environment. Use Ubuntu on WSL.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(TASKHUB_BUILD_TESTS "Build TaskHub tests" ON)

find_package(SQLite3 REQUIRED)
find_package(Threads REQUIRED)

include(FetchContent)
find_package(nlohmann_json 3.11.3 QUIET)
if(NOT nlohmann_json_FOUND)
    FetchContent_Declare(
        nlohmann_json
        URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

add_library(taskhub_core
    src/task.cpp
    src/sqlite_task_repository.cpp
    src/task_service.cpp
)
target_include_directories(taskhub_core PUBLIC include)
target_link_libraries(taskhub_core PUBLIC SQLite::SQLite3 Threads::Threads)
target_compile_options(taskhub_core PRIVATE -Wall -Wextra -Wpedantic)

add_library(taskhub_http
    src/http_server.cpp
    src/api_router.cpp
)
target_include_directories(taskhub_http PUBLIC include)
target_link_libraries(taskhub_http PUBLIC taskhub_core Threads::Threads nlohmann_json::nlohmann_json)
target_compile_options(taskhub_http PRIVATE -Wall -Wextra -Wpedantic)

add_executable(taskhub src/main.cpp)
target_link_libraries(taskhub PRIVATE taskhub_http)

if(TASKHUB_BUILD_TESTS)
    enable_testing()
    add_executable(taskhub_tests tests/taskhub_tests.cpp)
    target_link_libraries(taskhub_tests PRIVATE taskhub_http)
    target_compile_options(taskhub_tests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME taskhub_tests COMMAND taskhub_tests)
endif()
