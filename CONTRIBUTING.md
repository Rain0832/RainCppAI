# RainCppAI 贡献指南

> 本文件是项目的开发规范入口，所有贡献者（包括 AI Agent）必须遵守。
> 项目目录和架构详见 `README.md`。代码风格详见 `DEVELOP_STANDARD.md`。

---

## 一、模块依赖规则

```
HttpServer/       ← 纯网络库，不依赖任何业务模块
AIServerCore/     ← 业务编排层，依赖 HttpServer + AIEngine
AIEngine/         ← AI 工具库，零 HttpServer / AIServerCore 依赖
web/              ← 前端资源，无 C++ 编译期依赖
```

- HttpServer/ 不得引用 AIEngine/ 或 AIServerCore/ 的任何头文件
- AIEngine/ 不得引用 HttpServer/ 或 AIServerCore/ 的任何头文件
- 头文件与实现文件目录结构必须保持镜像关系

---

## 二、头文件管理

- 每个头文件必须有 `#pragma once`
- Include 路径和顺序详见 `DEVELOP_STANDARD.md` §2.2

---

## 三、Git 提交规范

格式：
```
【模块名】type: 描述（可中文）
```

模块名：HttpServer / AIServerCore / AIEngine / web / DB / MCP / Docs / Infra

type：feat / fix / refactor / perf / chore / docs

示例：
```
【DB】fix: pushMessageToMysql 改用 Prepared Statement 防注入
【MCP】refactor: 工具声明从硬编码改为 config.json 配置化
【AIEngine】feat: Agent Loop 增加最大步数配置项
```

---

## 四、依赖管理原则

> 依赖清单（版本 + 用途）见 `README.md` "环境要求"。

| 依赖 | 类别 | 可否移除 |
|------|------|----------|
| muduo | 网络底层 | 核心 |
| OpenSSL | SSL/TLS | 核心 |
| libcurl | AI API 调用 | 核心 |
| nlohmann/json | JSON 解析 | 核心 |
| MySQL Connector | 数据库 | 核心 |
| SimpleAmqpClient + rabbitmq-c | 异步消息队列 | 可选 |
| OpenCV | 图像前处理 | 可选 |
| ONNX Runtime | 端侧图像识别 | 可选 |

新增依赖必须：在 `README.md` 依赖表登记版本 → 在本表登记类别 → CMake 设为可选 → CHANGELOG 说明原因。

---

## 五、CHANGELOG 维护

每次完成功能后更新 CHANGELOG.md，格式见该文件。

---

## 六、AI Agent 开发规则

1. 改代码前先读头文件，理解接口再动实现
2. 不得新增全局变量
3. 不得破坏现有接口签名
4. 每次修改必须更新 CHANGELOG.md
5. 不得引入未登记的新依赖
6. SQL 操作必须用 Prepared Statement，禁止字符串拼接
7. 新增 Handler 必须在 `ChatServer::registerRoutes()` 注册路由
8. MCP 新增工具必须同步更新 config.json
9. 开始 PLAN 前必须阅读 `AGENT.md` 并遵循其流程
