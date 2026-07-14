# Day 1：Ubuntu、TaskHub 请求链路与 SQLite

日期：2026-07-14

## 今日目标

今天完成了 Ubuntu WSL 的基础配置，运行并观察 TaskHub，理解一次 HTTP 请求如何经过不同代码层次进入 SQLite，同时阅读了任务模型、业务服务和 SQLite 仓储的核心代码。

## 1. Ubuntu 与 WSL 基础

Ubuntu 是常用的 Linux 发行版。WSL 让 Ubuntu 运行在 Windows 中，可以使用 Linux 的终端、文件系统和开发工具。

需要掌握：

- Linux 路径使用 `/`，区分大小写。
- Linux 用户主目录通常是 `/home/用户名`，可用 `~` 表示。
- Windows C 盘在 WSL 中对应 `/mnt/c`。
- `sudo` 用管理员权限执行命令；输入密码时终端不会显示字符。
- `apt` 用来更新软件列表和安装软件。

今天使用的常用命令：

```bash
whoami                           # 查看当前用户
pwd                              # 查看当前目录
ls -la                           # 查看目录内容，包括隐藏文件
cd ~                             # 回到 Linux 用户主目录
cd ..                            # 返回上一级目录
find . -name "*.cpp"             # 查找 C++ 文件
grep -R "TaskService" src include tests  # 在代码中搜索文本
sudo apt update                  # 更新软件包索引
```

进入项目目录：

```bash
cd "/mnt/c/Users/ROG/Documents/Learning Project"
```

## 2. 构建、测试与运行项目

初始化开发工具并验证项目：

```bash
bash scripts/bootstrap-ubuntu.sh
bash scripts/verify.sh
```

启动服务：

```bash
./build/verify/taskhub /tmp/taskhub-learning.db 8080
```

这条命令包含三个部分：

- `./build/verify/taskhub`：编译后的 C++ 服务程序。
- `/tmp/taskhub-learning.db`：SQLite 数据库文件。
- `8080`：服务监听的端口。

服务器会一直等待请求，所以启动后终端不会立即返回提示符。这不是卡死。需要打开第二个终端，用 `curl` 充当客户端。

```bash
bash scripts/api-smoke-test.sh
curl -i http://127.0.0.1:8080/stats
```

停止服务器使用 `Ctrl + C`。

## 3. HTTP 与 API

HTTP 方法表达客户端想执行的操作：

| 方法 | 用途 | TaskHub 示例 |
| --- | --- | --- |
| `GET` | 读取数据 | `GET /tasks`、`GET /stats` |
| `POST` | 创建数据 | `POST /tasks` |
| `PATCH` | 修改部分数据 | `PATCH /tasks/{id}` |
| `DELETE` | 删除数据 | `DELETE /tasks/{id}` |

常见状态码：

- `200 OK`：查询或更新成功。
- `201 Created`：创建成功。
- `204 No Content`：删除成功，没有响应体。
- `400 Bad Request`：输入不合法。
- `404 Not Found`：路由或任务不存在。
- `500 Internal Server Error`：服务或数据库发生异常。

## 4. 一次请求的完整路径

```text
curl / 客户端
    ↓ HTTP 请求
HttpServer
    ↓ HttpRequest
ApiRouter
    ↓ 领域请求
TaskService
    ↓ TaskRepository 接口
SqliteTaskRepository
    ↓ SQL
SQLite 数据库文件
```

各层职责：

- `HttpServer`：接收网络连接，解析和发送 HTTP 报文。
- `ApiRouter`：判断方法与路径，解析 JSON，生成状态码和 JSON 响应。
- `TaskService`：执行标题校验、更新校验和状态统计等业务规则。
- `TaskRepository`：声明数据访问能力，不指定具体数据库。
- `SqliteTaskRepository`：执行 SQL，读写 SQLite。

分层的意义是让每个类只负责一类问题。以后将 SQLite 换成 MySQL 时，HTTP 路由和大部分业务逻辑不需要重写。

## 5. 任务模型与现代 C++

`Task` 包含：

```text
id、title、description、status、created_at、updated_at
```

任务状态是枚举：

```cpp
TaskStatus::Todo
TaskStatus::Doing
TaskStatus::Done
```

`to_string` 使用 `switch` 将枚举转换为 `"todo"`、`"doing"`、`"done"`；`task_status_from_string` 完成反向转换。

`CreateTask` 不是构造函数，而是创建请求的数据结构，只包含 `title` 和 `description`。新任务的状态由数据库默认设为 `todo`。

`std::optional` 表示一个值可能存在，也可能不存在：

- `find_by_id` 返回 `std::nullopt`：没有找到对应任务。
- `list(std::nullopt)`：没有状态筛选条件，查询全部任务。
- `UpdateTask` 的字段是可选的，只修改请求提供的字段。

## 6. `TaskService` 的职责

`TaskService` 负责业务规则，不负责 SQL：

- `create`：校验标题和描述，再调用仓储创建任务。
- `find_by_id`：拒绝无效 ID，再调用仓储查询。
- `list`：调用仓储获取任务列表。
- `stats`：遍历全部任务，统计三种状态和总数。
- `update`：要求至少提供一个字段，并校验新值。
- `remove`：检查 ID 后调用仓储删除。

`stats()` 的核心思路：

```text
repository_->list(std::nullopt)
    ↓ 获得全部任务
for 循环遍历
    ↓
switch 判断状态
    ↓
TaskStats 保存 todo、doing、done、total
```

## 7. SQLite 与仓储层

SQLite 是轻量级数据库，数据保存在单个文件中，不需要单独启动数据库服务器。

进入数据库：

```bash
sqlite3 /tmp/taskhub-learning.db
```

常用命令：

```sql
.databases
.tables
.schema tasks
.headers on
.mode column
SELECT id, title, status FROM tasks;
.quit
```

SQL 查询没有结果，可能表示表中没有数据；`INSERT`、`UPDATE`、`DELETE` 成功时默认也不会打印提示，可用 `SELECT changes();` 检查影响行数。

`SqliteTaskRepository` 中的重要函数：

- 构造函数：打开数据库、设置等待时间并创建 `tasks` 表。
- 析构函数：关闭数据库连接。
- `migrate`：执行 `CREATE TABLE IF NOT EXISTS`。
- `create`：执行 `INSERT`，再查询并返回完整任务。
- `find_by_id`：按 ID 查询，找不到时返回 `std::nullopt`。
- `list`：查询全部任务或按状态筛选。
- `update`：保留未提供字段，更新提供的字段。
- `remove`：执行 `DELETE`，根据影响行数返回是否删除成功。

辅助代码：

- `Statement`：用 RAII 管理 SQLite 预编译语句，析构时自动调用 `sqlite3_finalize`。
- `bind_text`：把字符串安全绑定到 SQL 的 `?` 参数，避免直接拼接 SQL。
- `row_to_task`：把数据库的一行转换成 C++ `Task`。
- `throw_on_error`：将 SQLite 错误转换为 `RepositoryError`。

仓储中的 `std::mutex` 防止多个请求线程同时操作共享数据库连接。`find_by_id_unlocked` 不自行加锁，供已经持有锁的 `create` 和 `update` 调用，从而避免同一线程重复锁住普通互斥锁。

## 8. 今日应具备的能力

- [x] 能打开 Ubuntu，理解 Linux 与 Windows 路径的对应关系。
- [x] 能进入项目目录并运行构建、测试和服务。
- [x] 能用 `curl` 调用 TaskHub API。
- [x] 能打开 SQLite 文件并查询 `tasks` 表。
- [x] 能说明一次请求从 HTTP 到 SQLite 的完整路径。
- [x] 能区分路由层、业务层和仓储层的职责。
- [x] 能解释 `Task`、`CreateTask`、`TaskStatus` 和 `std::optional` 的用途。
- [x] 能概述 SQLite 仓储各函数的作用。

## 自测问题

1. 为什么 `ApiRouter` 不应该直接写 SQL？
2. 为什么 `/stats` 使用 `GET` 而不是 `POST`？
3. `list(std::nullopt)` 表达什么含义？
4. 为什么创建任务时 `description` 可以为空，但 `title` 不能全是空格？
5. 为什么 `Statement` 禁止拷贝？
6. `find_by_id_unlocked` 为什么不使用 `lock_guard`？
7. SQLite 数据库文件与 MySQL 数据库服务器有什么区别？

## 下一步

下一次学习将使用 gdb 在 `TaskService::create` 设置断点，观察真实请求的调用栈、参数变化以及任务进入 SQLite 的过程。
