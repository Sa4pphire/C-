# TaskHub 实践实验手册

所有命令默认在 Ubuntu WSL 的项目根目录执行。每个实验都应把“观察结果”和“为什么”写入 `progress-log.md`。

## 实验 1：环境与干净构建

```bash
bash scripts/bootstrap-ubuntu.sh
bash scripts/verify.sh
```

验证：编译器、CMake、SQLite 和 27 个测试均可用。阅读脚本，说明为什么使用独立的 `build/verify` 目录，以及 `-Werror` 的作用。

## 实验 2：Linux 命令组合

```bash
pwd
find include src tests -type f
grep -R "lock_guard" include src tests
grep -R "TaskService" include src tests | wc -l
ps -ef | grep taskhub
echo $?
ss -ltnp | grep 8080
```

练习目标：解释管道把什么输出交给下一条命令；解释退出码 `0` 与非 `0` 的区别。

## 实验 3：gdb 跟踪创建请求

终端 A：

```bash
cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug
gdb --args ./build/debug/taskhub /tmp/taskhub-debug.db 8080
```

进入 gdb 后：

```text
break taskhub::TaskService::create
run
```

终端 B：

```bash
curl -i -X POST http://127.0.0.1:8080/tasks \
  -H 'Content-Type: application/json' \
  -d '{"title":"gdb practice","description":"trace one request"}'
```

断点命中后在终端 A 执行：

```text
bt
frame 0
info args
print request.title
next
continue
```

记录：调用栈、`request.title`、当前线程编号，以及校验发生在 SQL 之前的原因。

## 实验 4：HTTP API 冒烟测试

终端 A：

```bash
./build/verify/taskhub /tmp/taskhub-smoke.db 8080
```

终端 B：

```bash
bash scripts/api-smoke-test.sh
```

脚本依次覆盖健康检查、创建、列表、单项查询、更新、统计和删除。随后手工发送非法 JSON、未知状态和不存在 ID，分别确认 `400` 与 `404`。

## 实验 5：SQLite 与事务

先通过 API 创建两条任务，再打开数据库：

```bash
sqlite3 /tmp/taskhub-smoke.db
```

在 SQLite 交互环境执行：

```sql
.schema tasks
.headers on
.mode column
SELECT id, title, status FROM tasks ORDER BY id;
EXPLAIN QUERY PLAN SELECT * FROM tasks WHERE status = 'todo';

BEGIN;
UPDATE tasks SET status = 'done' WHERE id = 1;
SELECT id, status FROM tasks WHERE id = 1;
ROLLBACK;
SELECT id, status FROM tasks WHERE id = 1;
```

验证：回滚后状态恢复。思考：`status` 查询很多时是否需要索引？索引会给写入带来什么成本？

## 实验 6：并发与 ThreadSanitizer

```bash
bash scripts/verify-tsan.sh
```

阅读并发创建测试，找到 16 个线程的创建、等待和最终断言。实验性注释仓储中一个写操作的 `lock_guard`，重新运行检测并记录结果，然后立即撤销实验改动。

注意：部分 WSL 或虚拟化环境可能无法运行 ThreadSanitizer。若出现运行时映射错误，记录完整错误；普通构建和测试仍必须通过。

## 实验 7：代码格式与交付检查

```bash
find include src tests -type f \( -name '*.h' -o -name '*.cpp' \) \
  -print0 | xargs -0 clang-format --dry-run --Werror
git status --short
git log --oneline --decorate --graph
git diff master...HEAD
```

完成标准：格式检查、严格警告构建和测试全部通过；工作区只包含你明确知道用途的改动。
