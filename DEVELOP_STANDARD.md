# RainCppAI 开发规范

> 适用于本项目的 C++ 编码风格指南，综合现有代码习惯 + Google C++ Style Guide + 项目特定约定。
> 所有贡献者请遵守此规范。**最后更新：2026-06-13**

---

## 1. 命名约定

### 1.1 类名 — `PascalCase`

```cpp
// ✅ 正确
class HttpServer { };
class AIStrategy { };
class ChatSendHandler { };
class DbConnectionPool { };

// ❌ 错误
class httpServer { };
class AI_Strategy { };
```

### 1.2 函数 / 方法名 — `camelCase`

```cpp
// ✅ 正确
void handleRequest();
void sendResponse();
std::string getSessionId();
bool isExpired();

// ❌ 错误
void HandleRequest();
void send_response();
```

### 1.3 成员变量 — `snake_case_`（尾部下划线）

```cpp
// ✅ 正确
class Session {
private:
    std::string session_id_;
    int max_age_;
    std::chrono::steady_clock::time_point created_at_;
};

// ❌ 错误
std::string sessionId_;    // camelCase_
int m_maxAge;              // m_ 前缀
int max_age;               // 无下划线（与局部变量混淆）
```

### 1.4 局部变量 / 函数参数 — `snake_case`（无下划线后缀）

```cpp
// ✅ 正确
void process(int user_id, const std::string& session_key) {
    auto now = std::chrono::steady_clock::now();
    int retry_count = 0;
}

// ❌ 错误
void process(int userId, const std::string& sessionKey) {
    auto now_ = std::chrono::steady_clock::now();
}
```

### 1.5 常量 / 枚举 — `kPascalCase` 或 `UPPER_CASE`

```cpp
// 枚举值 — kPascalCase（推荐）
enum class HttpMethod {
    kGet,
    kPost,
    kPut,
    kDelete
};

// 或 UPPER_CASE（SSL/TLS 等协议常量）
enum class SSLVersion {
    TLS_1_0,
    TLS_1_1,
    TLS_1_2,
    TLS_1_3
};

// 编译期常量 — UPPER_CASE
constexpr int MAX_SESSIONS = 500;
constexpr size_t BUFFER_SIZE = 4096;
```

### 1.6 命名空间 — `小写单词`

命名空间命名遵循**简洁描述性**原则，不使用品牌式前缀。
各模块的具体命名空间定义见对应 `TECHDOC.md`。

```cpp
// 示例：顶层命名空间（与模块对应）
namespace http { }
namespace ai { }
namespace chat { }

// 示例：子命名空间（http 下）
namespace http::session { }
namespace http::router { }
namespace http::middleware { }
namespace http::ssl { }
```

**规则**：
- 禁止使用品牌式 / 伞式前缀（如 `rain::`）
- 禁止在头文件中 `using namespace` 指令
- `.cpp` 中允许 `using namespace`（仅在需要时）

### 1.7 文件名

| 类型 | 约定 | 示例 |
|------|------|------|
| 头文件 | `PascalCase.h` | `HttpServer.h`、`AIStrategy.h` |
| 源文件 | `PascalCase.cpp` | `HttpServer.cpp`、`AIStrategy.cpp` |
| 目录 | `lowercase` 或 `camelCase` | `router/`、`middleware/` |

---

## 2. 头文件规范

### 2.1 Include Guard — `#pragma once`

```cpp
// ✅ 推荐（所有现代编译器支持）
#pragma once

// ❌ 不推荐（易写错）
#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H
// ...
#endif
```

### 2.2 Include 路径 & 顺序

#### 路径规则

项目头文件**不包含 `include/` 目录名**，路径起点为各模块的 `include/` 根目录。
CMake 须配置对应的 `include_directories`。

```cpp
// ✅ 跨模块用路径
#include "http/HttpRequest.h"         // → HttpServer/include/http/HttpRequest.h
#include "llm/AIHelper.h"             // → AIEngine/include/llm/AIHelper.h
#include "server/ChatServer.h"        // → AIServerCore/include/server/ChatServer.h

// ✅ 同模块内也用路径（不依赖当前文件位置）
#include "router/Router.h"

// ❌ 禁止裸文件名
#include "AIHelper.h"

// ❌ 禁止冗长完整路径
#include "HttpServer/include/http/HttpServer.h"

// ❌ 禁止相对路径回溯
#include "../../../include/middleware/cors/CorsMiddleware.h"
```

#### 顺序（Google 风格 + 组内字母序）

五组顺序不变，**每组内部按字母序排列**（大写优先于小写，`_` 在字母之前）。

```cpp
// 1. 对应的头文件 (foo.cpp → foo.h)
#include "Foo.h"

// 2. C 系统头文件（字母序）
#include <sys/types.h>
#include <unistd.h>

// 3. C++ 标准库头文件（字母序）
#include <memory>
#include <string>
#include <vector>

// 4. 其他第三方库头文件（字母序）
#include <muduo/net/TcpServer.h>
#include <nlohmann/json.hpp>

// 5. 本项目头文件（字母序）
#include "http/HttpRequest.h"
#include "llm/AIHelper.h"
#include "router/Router.h"
#include "utils/ThreadPool.h"
```

### 2.3 前向声明优先

```cpp
// ✅ 头文件中尽量用前向声明代替 #include
class TcpConnection;
namespace muduo { namespace net { class EventLoop; } }

// 在 .cpp 中再 #include 完整定义
```

### 2.4 内联函数

- ≤ 10 行的简单函数可内联在头文件中
- 构造函数 / 析构函数一般不内联（除非为空）

---

## 3. 作用域与智能指针

### 3.1 优先使用 `std::shared_ptr` / `std::unique_ptr`

```cpp
// ✅ 正确
std::shared_ptr<AIHelper> getHelper(const std::string& session_id);
std::unique_ptr<HttpResponse> createResponse();

// ❌ 禁止裸 new / delete
AIHelper* helper = new AIHelper();  // 禁止
```

### 3.2 `enable_shared_from_this` 模式

```cpp
class Session : public std::enable_shared_from_this<Session> {
    // 允许在成员函数中返回自身的 shared_ptr
};
```

---

## 4. 并发规范

### 4.1 锁选择

| 场景 | 锁类型 |
|------|--------|
| 读多写少 | `std::shared_mutex` + `shared_lock` / `unique_lock` |
| 读写均衡 / 写多 | `std::mutex` + `lock_guard` |
| 简单标志位 | `std::atomic<bool>` / `std::atomic<int>` |

### 4.2 锁粒度

```cpp
// ✅ 正确：最小锁范围
{
    std::shared_lock lock(mutex_);
    auto it = map_.find(key);
}  // 锁在此释放

// 后续操作不加锁
process(it->second);

// ❌ 错误：锁范围过大
std::lock_guard lock(mutex_);
auto it = map_.find(key);
process(it->second);  // 长时间操作在锁内执行
```

### 4.3 线程池

```cpp
// 使用 ThreadPool 处理耗时任务，IO 线程不阻塞
auto future = thread_pool_.enqueue([this, session_id]() {
    return ai_helper_->chat(session_id, message);
});
// 通过 runInLoop 回调回 IO 线程
```

---

## 5. 错误处理

### 5.1 数据库 / 网络错误

```cpp
// ✅ 使用 try-catch + 日志
try {
    auto conn = pool_->getConnection();
    conn->execute(sql);
} catch (const std::exception& e) {
    LOG_ERROR("DB error: {}", e.what());
    return HttpResponse::internalServerError();
}
```

### 5.2 超时处理

```cpp
// 等待可配置超时，防止永久阻塞
if (!cv_.wait_for(lock, std::chrono::seconds(3), [this] { return !pool_.empty(); })) {
    throw std::runtime_error("DB connection pool timeout");
}
```

---

## 6. 注释规范

### 6.1 文件头注释（Doxygen 风格）

```cpp
/**
 * @file HttpServer.h
 * @brief HTTP 服务器核心头文件
 *
 * 基于 muduo 网络库的高性能 HTTP 服务器实现。
 * 采用事件驱动架构，支持多线程并发处理。
 */
```

### 6.2 函数注释

```cpp
/**
 * @brief 处理 HTTP 请求
 * @param req HTTP 请求对象
 * @param conn TCP 连接指针
 * @return HTTP 响应对象
 */
HttpResponse handleRequest(const HttpRequest& req, const TcpConnectionPtr& conn);
```

### 6.3 成员变量注释

```cpp
int max_sessions_;       ///< 最大会话数（默认 500）
bool is_running_;        ///< 服务器运行状态
```

---

## 7. 代码格式化

- **缩进**：4 空格（不使用 Tab）
- **行宽**：最大 120 字符
- **大括号**：K&R 风格（左括号不换行）
- **指针 / 引用**：`Type* ptr`、`Type& ref`（`*` 和 `&` 紧贴类型）

```cpp
// ✅ 正确
if (condition) {
    doSomething();
} else {
    doOther();
}

void foo(int* ptr, const std::string& str) {
    // ...
}
```

---

## 8. CMake 规范

```cmake
# 最低版本
cmake_minimum_required(VERSION 3.16)

# C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2")

# 源文件按模块分组
add_executable(http_server
    HttpServer/src/http/HttpServer.cpp
    AIServerCore/src/server/ChatServer.cpp
    # ...
)
```

---

## 9. 设计原则

1. **RAII 优先** — 资源由对象生命周期管理，禁止裸 `new`/`delete`
2. **接口与实现分离** — 头文件只暴露必要的 public API
3. **组合优于继承** — 优先使用组合模式（如 Strategy 模式）
4. **单一职责** — 每个类只做一件事；函数不超过 50 行
5. **显式优于隐式** — 用 `enum class` 而非 `int`；用命名常量而非魔数
