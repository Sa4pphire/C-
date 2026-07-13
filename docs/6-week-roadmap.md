# TaskHub 六周学习路线

每周安排 10–14 小时。建议每次学习都遵循：**30 分钟阅读 → 60 分钟编码 → 20 分钟调试/记录 → 提交 Git**。

## 第 1 周：从 C 过渡到现代 C++

- 配好 WSL、CMake、g++、gdb 与 Git，并完成 README 的构建命令。
- 练习 `const`、引用、`std::string`、`std::vector`、范围 `for`、函数重载和异常。
- 阅读 `src/task.cpp` 与 `include/taskhub/task.h`，为 `TaskStatus` 增加一个自己设计的辅助函数。
- 重点问题：为什么 `std::vector` 比手写动态数组安全？引用与指针各适合何时使用？

## 第 2 周：对象、STL 与 RAII

- 学习构造/析构、封装、智能指针、接口类和移动语义的用途。
- 阅读 `Statement` 与 `SqliteTaskRepository`，说明它们何时申请资源、何时自动释放资源。
- 用 `<algorithm>` 为列表添加一个纯内存排序练习。
- 重点问题：为什么仓库类禁止拷贝？为什么析构函数不应抛出异常？

## 第 3 周：服务分层与并发

- 阅读 `TaskService` 的校验逻辑和 `TaskRepository` 接口；画出请求的数据流。
- 学习 `std::thread`、`std::mutex`、`std::lock_guard`，运行并理解并发创建测试。
- 用 gdb 在 `TaskService::create` 设置断点，观察请求如何进入 SQLite。
- 重点问题：`lock_guard` 防止了什么问题？接口隔离如何让测试更容易？

## 第 4 周：HTTP 与 API

- 阅读 `HttpServer`：请求行、请求头、`Content-Length`、响应状态码的处理过程。
- 用 curl 逐一调用六个 API；观察 SQLite 文件在重启后仍保留数据。
- 给 API 增加一个只读字段或一个新的查询参数，并相应补测试。
- 重点问题：`400`、`404` 与 `500` 应怎样区分？

## 第 5 周：测试与质量

- 阅读 `tests/taskhub_tests.cpp`，把每个测试归为单元、集成或并发测试。
- 新增至少三个失败路径的测试，再实施最小修复。
- 用 `ctest --output-on-failure` 保持每次改动后测试全绿。
- 运行 `clang-format -i include/taskhub/*.h src/*.cpp tests/*.cpp` 后检查变更。

## 第 6 周：交付与讲解

- 独立从空目录重新构建项目，并按照 README 启动服务。
- 把架构、接口、测试结果和一项自己实现的功能整理进简历项目描述。
- 练习两分钟项目介绍；不要只背术语，要能结合代码解释 RAII、线程安全和分层。
- 下一阶段依次学习：Linux I/O、TCP、线程池、MySQL、Redis、Docker、成熟 C++ Web 框架。
