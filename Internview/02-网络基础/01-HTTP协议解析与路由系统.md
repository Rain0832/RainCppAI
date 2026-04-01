# HTTP 协议解析、路由系统与 SSL/TLS 集成

> 来源：RainCppAI 项目面试追问整理

---

## 一、HTTP 协议解析 & 状态机

### Q1: HTTP 解析为什么是状态机？状态在哪里？具体如何实现？

HTTP 报文有严格的分段结构（请求行 → 请求头 → 请求体），但 TCP 是流式协议，数据到达的边界和 HTTP 报文的边界不一定对齐（粘包/拆包问题）。因此需要有限状态机（FSM）来记住当前解析进度。

**状态定义**：

```cpp
enum HttpRequestParseState {
    kExpectRequestLine,  // 等待解析请求行
    kExpectHeaders,      // 等待解析请求头
    kExpectBody,         // 等待解析请求体
    kGotAll,             // 解析完成
};
```

**状态存储位置**：`HttpContext` 对象通过 `conn->setContext()` 绑定到每条 TCP 连接上，每条连接独立维护解析状态。

**状态转换流程**：

```
连接建立 → kExpectRequestLine
    ↓ 找到 \r\n → processRequestLine() 解析出 Method/Path/Version
kExpectHeaders
    ↓ 循环找 \r\n，用 ':' 分割 key:value，遇到空行判断：
    ↓ GET/HEAD/DELETE → kGotAll | POST/PUT → 读 Content-Length → kExpectBody
kExpectBody
    ↓ buf->readableBytes() >= Content-Length → kGotAll
kGotAll → 调用 onRequest() → reset() → 回到 kExpectRequestLine (Keep-Alive)
```

**处理粘包/拆包的关键**：在 kExpectBody 阶段，如果数据不完整，返回 true 但保持状态不变，下次 onMessage 继续解析。

### Q2: chunked 编码和 multipart/form-data 支持

当前项目不完整支持 chunked 编码，仅依赖 Content-Length。如果要支持 chunked，需在状态机中增加 `kExpectChunkedBody` 状态。

multipart/form-data 的做法是通过 Content-Length 接收完整 body 后，在 Handler 层面解析 boundary 分隔的各部分。

---

## 二、路由系统设计

### Q3: 双匹配策略

| 匹配方式 | 适用场景 | 时间复杂度 | 实现 |
|----------|---------|-----------|------|
| 精确匹配 | 固定路径 `/login` | O(1) | `unordered_map<RouteKey, Handler>` 哈希表 |
| 正则匹配 | 动态路径 `/users/:id` | O(n) | `vector<RouteHandlerObj>` 线性遍历 |

**精确匹配为什么 O(1)**：`unordered_map` 底层是哈希表，计算 `RouteKeyHash(method, path)` 得到 bucket 索引，平均 O(1)。

**正则匹配为什么 O(n)**：必须逐个遍历所有注册的正则路由进行 `std::regex_match`。

**替代方案**：Trie 树 / Radix Tree（Go Gin 框架采用），复杂度 O(k)，k=路径段数。

---

## 三、中间件机制

### Q4: 中间件链的洋葱模型

```
请求进入
  → Middleware A.before()
  → Middleware B.before()
  → Middleware C.before()
  ═══ Handler.handle() ═══
  → Middleware C.after()  (逆序)
  → Middleware B.after()
  → Middleware A.after()
响应返回
```

- **before**：Handler 前执行，负责请求修改/校验（检查 token、CORS 预检、请求日志）
- **after**：Handler 后执行，负责响应增强（添加 CORS 头、压缩、响应日志）
- 中间件是**串行执行**的，可通过抛出 HttpResponse 异常短路

---

## 四、SSL/TLS 集成

### Q5: muduo 非阻塞 IO + OpenSSL 阻塞 IO 集成

**问题**：muduo 的 socket 是 `O_NONBLOCK`，OpenSSL 默认使用阻塞式 IO，两者直接结合会出错。

**方案——自定义 BIO 回调**：不让 OpenSSL 直接碰 socket，让它读写 muduo 的 Buffer。

```
SSL_write() → [自定义 BIO] → muduo Buffer → muduo::conn->send() → 网络
网络 → muduo Buffer → [自定义 BIO] → SSL_read() → 解密数据
```

三个关键回调：
- `bioWrite`：SSL 加密后的数据交给 muduo 异步发送
- `bioRead`：从 muduo Buffer 读数据给 SSL 解密，数据不够时设置 `BIO_set_retry_read`
- `bioCtrl`：处理 flush 等控制操作

### Q6: SSL 握手 vs TCP 三次握手

```
时间线：
  ① TCP 三次握手 (SYN → SYN+ACK → ACK)        内核完成
  ② SSL/TLS 握手 (ClientHello → ... → Finished) 应用层
  ③ HTTP 通信 (加密的请求/响应)
```

握手状态机：`HANDSHAKE → ESTABLISHED → SHUTDOWN/ERROR`

---

## 复习要点速记

- [ ] 状态机解决 TCP 流式协议和 HTTP 分段结构的阻抗不匹配
- [ ] 每条连接独立状态（HttpContext），避免全局锁
- [ ] reset() 支持 HTTP Keep-Alive 连接复用
- [ ] 精确匹配 O(1)：unordered_map 哈希表
- [ ] 正则匹配 O(n)：线性遍历 + regex_match
- [ ] 替代方案：Trie/Radix Tree → O(k)
- [ ] 洋葱模型：before 顺序 → Handler → after 逆序
- [ ] SSL 集成：自定义 BIO 回调，桥接 OpenSSL 和 muduo Buffer
- [ ] TCP 握手在传输层（内核），SSL 握手在应用层（用户代码）
