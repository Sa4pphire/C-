# TaskHub

TaskHub 是一个用 **C++17** 编写的轻量任务管理 HTTP 服务。它将任务保存到 SQLite，采用路由、业务服务、仓库三个层次，并用互斥锁保护共享数据库连接。这是一个面向 C++ 后端实习的学习型项目：小而完整、可测试、可讲解。

## 技术与结构

```text
HTTP request
    │
    ▼
ApiRouter ──► TaskService ──► SqliteTaskRepository ──► SQLite
    │              │                    │
    └─ JSON        └─ validation         └─ RAII + mutex
```

- **C++17**：类、`std::optional`、智能指针、RAII、STL、线程与同步。
- **HTTP/JSON**：一个最小 HTTP/1.1 服务和 JSON API 路由。
- **SQLite**：启动时自动创建 `tasks` 表；重启后数据仍存在。
- **CMake / CTest**：可重复构建、自动执行 23 个单元与集成测试。

## Linux 环境准备

推荐在 Ubuntu WSL 中运行。安装完成 WSL 后，在 Ubuntu 终端执行：

在管理员 PowerShell 中安装 Ubuntu（安装系统功能后按提示重启）：

```powershell
wsl --install -d Ubuntu --web-download
```

首次重启后打开 Ubuntu，按提示创建 Linux 用户；随后在 Ubuntu 终端执行：

```bash
sudo apt update
sudo apt install -y build-essential cmake gdb git libsqlite3-dev curl
```

项目会通过 CMake 自动下载固定版本的 `nlohmann/json`。第一次配置需要可访问 GitHub。

## 构建、测试与启动

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
./build/taskhub taskhub.db 8080
```

服务启动后监听 `http://127.0.0.1:8080`。第一个参数是数据库文件，第二个参数是端口；两者都可省略。

## API 示例

| 方法 | 路径 | 说明 |
| --- | --- | --- |
| `GET` | `/health` | 健康检查 |
| `POST` | `/tasks` | 创建任务 |
| `GET` | `/tasks?status=todo` | 查询任务；状态可为 `todo`、`doing`、`done` |
| `GET` | `/tasks/{id}` | 查询单个任务 |
| `PATCH` | `/tasks/{id}` | 更新任务字段 |
| `DELETE` | `/tasks/{id}` | 删除任务 |

创建任务：

```bash
curl -i -X POST http://127.0.0.1:8080/tasks \
  -H 'Content-Type: application/json' \
  -d '{"title":"学习 RAII","description":"完成智能指针练习"}'
```

更新状态：

```bash
curl -i -X PATCH http://127.0.0.1:8080/tasks/1 \
  -H 'Content-Type: application/json' \
  -d '{"status":"doing"}'
```

成功响应统一使用 `{"data": ...}`，业务校验错误使用 `{"error":{"message":"..."}}`。空标题、未知状态、非法 JSON 返回 `400`；不存在的任务返回 `404`；数据库异常返回 `500`。

## 关键学习点

- `SqliteTaskRepository` 的析构函数负责关闭数据库；`Statement` 类负责析构时释放预编译语句。这是 RAII，而不是手动散落的资源释放。
- `TaskRepository` 是接口，`TaskService` 只依赖接口；以后可以替换成 MySQL 或内存实现。
- 共享 SQLite 连接由 `std::mutex` 保护，因此多个请求线程不会同时操作同一连接。
- HTTP 服务目前是一请求一线程的教学实现。生产环境应改为线程池、连接超时、结构化日志和成熟 HTTP 框架。

更多每天的学习任务见 [6-week-roadmap.md](docs/6-week-roadmap.md)，面试项目介绍见 [interview-pitch.md](docs/interview-pitch.md)。
