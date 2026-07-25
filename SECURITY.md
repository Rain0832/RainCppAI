# Security Policy

## 环境变量要求

| 变量名 | 用途 | 示例 |
|---|---|---|
| `DB_PASSWORD` | MySQL 数据库密码（优先级高于 config.json） | `export DB_PASSWORD=your_password` |
| `MCP_PYTHON` | MCP Python 解释器路径 | `export MCP_PYTHON=/root/RainCppAI/.venv/bin/python` |

`ConfigManager` 自动将 `config.json` 路径映射为环境变量：`db.password` → `DB_PASSWORD`，`mcp.python` → `MCP_PYTHON`。

## 依赖清单（C++ 运行时）

| 依赖 | 版本 | 用途 | 已知 CVE |
|---|---|---|---|
| OpenSSL | 3.0.13 | TLS/密码学 | 无严重 |
| libcurl | 8.5.0 | HTTP 客户端（调用 LLM API） | 无严重 |
| libsodium | 1.0.18 | argon2id 密码哈希 | 无严重 |
| OpenCV | 4.6.0 | 图像识别 | 低风险（仅本地使用） |
| ONNX Runtime | 系统安装 | ML 模型推理 | 低风险（仅本地使用） |
| mysqlcppconn8 | 系统安装 | MySQL 连接 | 需定期升级 |
| muduo | 源码编译 | HTTP 服务器 | 关注官方更新 |
| nlohmann/json | 头文件 | JSON 解析 | 无严重 |

## 报告安全漏洞

请发送邮件至项目维护者，勿在公开 Issue 中描述漏洞细节。
