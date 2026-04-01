# 智能指针、RAII、移动语义与模板编程

> 来源：RainCppAI 项目面试追问整理

---

## 一、shared_ptr vs unique_ptr 选择原则

| 语义 | 选择 | 关键问题 |
|------|------|---------|
| **独占所有权** | `unique_ptr` | 任意时刻只有一个拥有者？ |
| **共享所有权** | `shared_ptr` | 可能被多处同时持有？ |
| **观察不拥有** | `weak_ptr` | 只需访问不延长生命周期？ |

### 项目中的四个案例

1. **`shared_ptr<AIStrategy>`**：多个 AIHelper 可能同时持有同一策略对象
2. **`unique_ptr<ssl::SslContext>`**：SSL 上下文全局只有一个，只属于 HttpServer
3. **`shared_ptr<Session>`**：同一 Session 可能被多个并发请求引用
4. **连接池中的 `shared_ptr<DbConnection>`**：更合理的做法是池中用 `unique_ptr`，取出时用自定义删除器包装为 `shared_ptr`

### 常见选择场景

| 场景 | 选择 | 原因 |
|------|------|------|
| 工厂方法返回值 | `unique_ptr` | 所有权转移给调用者 |
| 容器元素 | `unique_ptr` | 容器独占元素 |
| 观察者模式被观察者 | `shared_ptr` + `weak_ptr` | 观察者不延长生命周期 |
| 多线程共享缓存 | `shared_ptr` | 多线程可能同时持有 |
| RAII 资源管理 | `unique_ptr` + 自定义删除器 | 如 `unique_ptr<FILE, decltype(&fclose)>` |

---

## 二、RAII

本质：构造获取资源，析构释放资源。智能指针是 RAII 的一种应用。

| 资源 | 获取 | 释放 | RAII 实现 |
|------|------|------|----------|
| 堆内存 | `new` | `delete` | `shared_ptr` / `unique_ptr` |
| 互斥锁 | `lock()` | `unlock()` | `lock_guard` / `unique_lock` |
| SSL 对象 | `SSL_new()` | `SSL_free()` | `SslConnection` 析构函数 |
| curl 句柄 | `curl_easy_init()` | `curl_easy_cleanup()` | 应封装为 `unique_ptr<CURL, CurlDeleter>` |
| 数据库连接 | `connect()` | 归还到池 | `shared_ptr` 自定义删除器 |

**curl RAII 封装示例**：
```cpp
struct CurlDeleter {
    void operator()(CURL* c) { curl_easy_cleanup(c); }
};
using CurlPtr = unique_ptr<CURL, CurlDeleter>;
CurlPtr curl(curl_easy_init());  // 离开作用域自动 cleanup
```

---

## 三、std::move 的本质

`std::move` = `static_cast<T&&>()` —— 将左值转换为右值引用的类型转换。

**移动本身不在 move 中发生**，而是在接收方的移动构造函数/移动赋值运算符中：
```cpp
string::string(string&& other) noexcept {
    data_ = other.data_;      // 窃取内部指针
    other.data_ = nullptr;    // 原对象置空
}
```

move 后原对象处于"有效但未指定"的状态（可析构和重新赋值，但不应使用其值）。

---

## 四、可变参数模板

C++11 引入的参数包（Parameter Pack）语法，允许模板接受任意数量、任意类型的参数。

```cpp
template<typename T, typename... Args>
void bindParams(PreparedStatement* stmt, int index, T&& value, Args&&... args) {
    stmt->setString(index, std::to_string(std::forward<T>(value)));
    bindParams(stmt, index + 1, std::forward<Args>(args)...);  // 递归展开
}
void bindParams(PreparedStatement* stmt, int index) {}  // 递归终止
```

**`std::forward<T>`**（完美转发）：保持参数的左值/右值属性不变传递。

| 优点 | 缺点 |
|------|------|
| 类型安全（编译期检查） | 编译错误信息极难读 |
| 零运行时开销（编译期展开） | 编译时间增加 |
| 接口简洁 | 代码膨胀（每种参数组合实例化一份） |

C++17 改进——折叠表达式：`((expr), ...)` 代替递归。

---

## 五、runInLoop 与 queueInLoop

```
runInLoop(cb):
  if (当前在 EventLoop 线程): cb() 同步执行
  else: queueInLoop(cb)

queueInLoop(cb):
  加入 pendingFunctors_ 队列 → wakeup() → 下次 loop() 中执行
```

本质是**线程间安全的任务投递机制**，保证某些操作只在特定线程中执行，避免加锁。

---

## 复习要点速记

- [ ] shared_ptr：共享所有权，引用计数，有额外开销
- [ ] unique_ptr：独占所有权，零开销，不可拷贝只能 move
- [ ] weak_ptr：观察不拥有，防止循环引用
- [ ] RAII：构造获取 + 析构释放，适用于所有资源
- [ ] move：类型转换 static_cast<T&&>，真正移动在移动构造/赋值中
- [ ] 可变参数模板：参数包 + 递归展开 + 完美转发
- [ ] runInLoop：条件同步 | queueInLoop：始终异步
