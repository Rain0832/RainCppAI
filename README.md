#  C++ AI应用开发项目 - AI应用服务平台

- **多模型适配（Strategy + Factory）**：统一抽象 `AIStrategy`，一键切换 **阿里百炼 / 百炼-RAG / 豆包 /（预留）本地 LLaMA/llama.cpp、GGUF**。
- **轻量级 MCP 思想落地**：通过 **配置化工具注册（`AIToolRegistry`）+ Prompt 协议化** 实现“模型判断→工具调用→二次回答”的 **两段式推理**，对齐 **Model Context Protocol** 的核心理念。
- **RAG 检索增强**：解析→分块→嵌入→ANN 检索（Faiss/Milvus 预留）→可选重排→**带引用回答**，支持 **知识库 ID** 配置化接入。
- **多会话管理**：从 **单用户单会话** 升级为 **单用户多会话**，`unordered_map<userId, map<sessionId, AIHelper>>` 精准隔离上下文。
- **语音链路（ASR/TTS）**：集成 **百度 TTS**（任务创建→轮询→回传 URL），ASR 接口封装预留；支持 **参数化语速/音色**。
- **异步化与可靠性**：**RabbitMQ** 承载持久化写库，前台 **同步写内存、异步入库**，避免主线程阻塞；幂等/重试机制可扩展。
- **全链路可维护**：`AIHelper` 重构，**对话/模型切换/消息入库** 一步到位；**配置驱动（`config.json`）** 管理工具清单与 Prompt 模板。
- **容器化交付**：**独立 v1/v2 Docker 镜像**，MySQL + RabbitMQ 一键拉起；环境一致、上手即跑。

## 本项目视频演示

![image](https://file1.kamacoder.com/i/web/2025-11-07_11-28-19.jpg)

![image](https://file1.kamacoder.com/i/web/2025-11-07_11-28-53.jpg)

![image](https://file1.kamacoder.com/i/web/2025-11-07_11-29-21.jpg)

## 架构图

![](https://file1.kamacoder.com/i/web/2025-10-15_16-19-50.jpg)

架构图展示了 自研 [C++ HTTP 服务框架](https://programmercarl.com/other/project_http.html) 如何将 AI 模型调用、图像识别、消息队列、数据库存储与多厂商模型 API 进行解耦，实现了高性能、可扩展、可私有化部署的 AI 应用平台。

整个系统从上到下可分为四层：

* 客户端层	用户通过 Web / 命令行 / 其他 SDK 发起请求（例如 AI 聊天、文档问答、图像识别等）
* 业务服务层（C++ 框架核心）	提供对话服务、图像识别服务、用户管理服务，是整个平台的核心逻辑层
* 数据与消息层	负责业务数据的存储、异步任务的转发与缓冲，提升系统稳定性与并发性能
* 推理与第三方平台层	对接多家 AI 大模型（阿里云、百度智能云、火山引擎等）以及本地推理引擎（ONNXRuntime）

## 流程图

![](https://file1.kamacoder.com/i/web/2025-10-15_16-22-16.jpg)

展示了整个系统从 客户端请求 → ChatServer 业务调度 → 多模型调用 → 异步消息入库 的全链路流程

一、总体架构思路

该系统基于[自研的 C++ HTTP 服务框架](https://programmercarl.com/other/project_http.html) 构建，是一个支持：

* 多模型接入（GPT / 通义 / 豆包 / 百炼 / 百川）
* 图像识别（ONNX + OpenCV）
* 语音识别与合成（ASR/TTS）
* 异步消息入库（RabbitMQ）
* 多会话管理
* MCP 工具协议化

的完整 AI 应用服务平台。

系统核心是 ChatServer，它负责：

* 接收客户端请求；
* 调用对应业务 Handler；
* 根据类型分发到不同 AI 模块（聊天、图像识别、语音）；
* 将结果异步入库或交由队列处理。

## 收获
做完这个项目，你可以学会

* **独立完成 C++ + 大模型 + RAG + 多模态 全链路开发**；
* **理解底层 HTTP、线程池、异步消息、模型推理之间的真实数据流**；
* **把“C++ 系统能力”和“AI 应用能力”结合在一起**。

更具体一些，你会真正理解一个 AI 平台的完整架构：

* 如何在 C++ 框架中封装 多模型策略层（GPT、通义、豆包、百炼）；
* 如何实现类似 MCP（Model Context Protocol） 的上下文管理；
* 如何用 RabbitMQ + 线程池 做异步入库和任务调度；
* 如何接入 语音识别（ASR）+ 语音合成（TTS）；
* 如何集成本地 ONNX 模型推理；
* 如何设计 多会话隔离与上下文管理；
* 如何让一个 AI 服务同时支持 云端模型与本地推理 模式。