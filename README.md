# Dr.Rain — 智能医疗 AI 助手

> Healthcare AI Assistant · 全栈 C++ 自研 HTTP 框架 · 多模型 LLM · SSE 流式 · MCP 协议 · 端侧视觉 · Admin 后台

[English](#english) | [中文](#chinese)

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![CMake](https://img.shields.io/badge/CMake-%E2%89%A5%203.16-blue.svg)](https://cmake.org/)
[![License](https://img.shields.io/badge/License-Apache%202.0-green.svg)](LICENSE)
[![Version](https://img.shields.io/badge/Version-v3.0.0-orange.svg)](CHANGELOG.md)

---

<a id="english"></a>
## 🇬🇧 English

### ✨ Core Features

| Module | Capabilities |
|--------|-------------|
| **AI Chat** | Multi-model LLM (Qwen / DouBao), SSE streaming, MCP Function Calling, Agent Loop |
| **Vision** | In-chat image upload → ONNX MobileNetV2 inference → Prompt injection → LLM response |
| **User System** | Invite-code registration, email verification (QQ SMTP), JWT auth, RBAC (admin/user/org) |
| **Security** | argon2id password hashing, login lockout, CORS / CSP / HSTS headers, error redaction |
| **Admin** | Real-time dashboard, user/invite-code/feedback management, log viewer, root auto-seeding |
| **Infrastructure** | Repository/Service layering, ConfigManager, GoogleTest, GitHub Actions CI/CD |

### 🏗 Architecture

```
Browser / Nginx → muduo Reactor → Middleware Chain → Router
  └─► AIServerCore (30+ Handlers)
        └─► AIEngine (LLM / MCP / Vision / TTS)
              └─► Storage → MySQL
```

### 🔧 Quick Start

```bash
# 1. Build
sudo apt install -y cmake g++ libssl-dev libcurl4-openssl-dev \
  libmysqlcppconn-dev libopencv-dev libsodium-dev
wget https://github.com/microsoft/onnxruntime/releases/download/v1.17.1/... -O onnxruntime.tgz
tar xzf onnxruntime.tgz -C /usr/local

cd RainCppAI && mkdir build && cd build
cmake .. && make -j$(nproc)

# 2. Configure
cp ../config.json.example ../config.json
# Edit config.json → set DB credentials + LLM API keys

# 3. Download models (optional — for vision)
bash ../scripts/download_models.sh

# 4. Run
./http_server -p 8088
```

> 📖 **深度文档请查阅 [Wiki](https://github.com/Rain0832/RainCppAI/wiki)**  — 架构设计、API 参考、部署指南、MCP 插件开发、项目结构、依赖清单。

---

<a id="chinese"></a>
## 🇨🇳 中文

### ✨ 核心能力

| 模块 | 功能 |
|------|------|
| **AI 对话** | 多模型 LLM（百炼/豆包）、SSE 流式输出、MCP Function Calling、Agent Loop |
| **视觉识别** | 聊天框内图片上传 → ONNX MobileNetV2 推理 → Prompt 注入 → LLM 自然回答 |
| **用户系统** | 邀请码注册、邮箱验证（QQ SMTP）、JWT 鉴权、RBAC 角色体系（admin/user/org） |
| **安全加固** | argon2id 密码哈希、登录失败锁定、CORS / CSP / HSTS 安全头、错误脱敏 |
| **管理后台** | 实时看板、用户/邀请码/反馈管理、日志查看器、Root 账号自动播种 |
| **工程基建** | Repository/Service 分层架构、ConfigManager 统一配置、GoogleTest、GitHub Actions CI |

### 🏗 架构

```
浏览器 / Nginx → muduo Reactor → 中间件链 → 路由分发
  └─► AIServerCore (30+ Handler)
        └─► AIEngine (LLM / MCP / 视觉 / 语音)
              └─► Storage → MySQL
```

### 🔧 快速开始

```bash
# 1. 编译
sudo apt install -y cmake g++ libssl-dev libcurl4-openssl-dev \
  libmysqlcppconn-dev libopencv-dev libsodium-dev
wget https://github.com/microsoft/onnxruntime/releases/download/v1.17.1/... -O onnxruntime.tgz
tar xzf onnxruntime.tgz -C /usr/local

cd RainCppAI && mkdir build && cd build
cmake .. && make -j$(nproc)

# 2. 配置
cp ../config.json.example ../config.json
# 编辑 config.json → 设置数据库凭据 + LLM API Key

# 3. 下载模型（可选 — 启用视觉识别）
bash ../scripts/download_models.sh

# 4. 启动
./http_server -p 8088
```

> 📖 **深度文档请查阅 [Wiki](https://github.com/Rain0832/RainCppAI/wiki)**  — 架构设计、API 参考、部署指南、MCP 插件开发、项目结构、依赖清单。

---

## 🤝 贡献

见 [`CONTRIBUTING.md`](CONTRIBUTING.md)（模块依赖规则、Commit 规范、开发流程）。
代码风格: [`DEVELOP_STANDARD.md`](DEVELOP_STANDARD.md)。

## 📄 许可证

[Apache License 2.0](LICENSE)

## 📖 参考资料

- [muduo — Linux 多线程服务端编程](https://github.com/chenshuo/muduo)
- [spdlog — Fast C++ logging](https://github.com/gabime/spdlog)
- [Model Context Protocol](https://modelcontextprotocol.io/)
- [ONNX Runtime C++ API](https://onnxruntime.ai/docs/api/c/)
- [阿里云百炼 API](https://help.aliyun.com/product/610100.html)
