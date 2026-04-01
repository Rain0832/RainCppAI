# Agent 架构、工具调用与端侧推理

> 来源：RainCppAI 项目面试追问整理

---

## 一、MCP 标准协议与各厂商实现

### 1.1 MCP 标准

MCP（Model Context Protocol）是 Anthropic 提出的标准化协议，让 LLM 能调用外部工具。
- 传输层：JSON-RPC 2.0，支持 stdio / HTTP+SSE
- 能力声明：Server 通过 `tools/list` 暴露可用工具
- 工具调用：Client 发起 `tools/call`，Server 执行并返回结果

### 1.2 各大模型公司对比

| 厂商 | 名称 | 实现方式 |
|------|------|---------|
| OpenAI | Function Calling | 模型原生 `tools` 参数，返回结构化 `tool_calls` JSON |
| Anthropic | Tool Use | 请求传 `tools` 数组，响应返回 `tool_use` block |
| Google | Function Calling | Gemini 原生支持 |
| 阿里 | Function Calling | 通义千问支持 `tools` 参数（OpenAI 兼容接口） |

### 1.3 项目中的"两段式推理"

**与标准做法的差异**：项目没有使用模型原生 Function Calling API，而是通过 Prompt Engineering 实现。

```
用户问题 → 注入工具列表到 Prompt → 第一次 LLM（判断是否需要工具）
                                      ├── 不需要 → 直接返回
                                      └── 需要 → 调用本地工具
                                                  ↓
                                          工具结果注入新 Prompt → 第二次 LLM → 最终回答
```

| 维度 | 项目做法 | 标准做法 |
|------|---------|---------|
| 工具声明 | Prompt 注入文本描述 | API `tools` 参数 |
| 工具调用 | 解析自由文本 JSON | 结构化 `tool_calls` 字段 |
| 可靠性 | 依赖 LLM 按格式输出 | 模型原生支持，稳定可靠 |
| 兼容性 | 适用于任何 LLM | 需要模型支持 Function Calling |

---

## 二、Agent 架构（ReAct 模式）

```
行业标准：用户问题 → Thought → Action → Observation → Thought → ... → Answer
项目实现：用户问题 → LLM 判断 → 调工具 → LLM 综合回答（最多两步）
```

升级方向：
- 两段式 → ReAct 循环（支持多步工具调用）
- Prompt 注入 → 标准 Function Calling
- 全量上下文 → 滑动窗口 + 摘要压缩

---

## 三、ONNX & 端侧推理

**ONNX**（Open Neural Network Exchange）：跨框架模型表示标准 + 高性能推理引擎。
```
PyTorch 训练 → 导出 .onnx → ONNX Runtime 推理（C++/Python）
```

**MobileNetV2**：Google 轻量级图像分类模型，参数量 ~3.4M，适合移动端。

项目流程：`图片(Base64) → OpenCV 解码 → 预处理 → ONNX Runtime(MobileNetV2) → 1000维softmax → Top-1 类别`

**自主 TTS/ASR 方案**：
- ASR：whisper.cpp（纯 C/C++ Whisper，无 Python 依赖）
- TTS：sherpa-onnx（C++ 原生支持）

---

## 复习要点速记

- [ ] MCP：Anthropic 标准协议，JSON-RPC 2.0，tools/list + tools/call
- [ ] Function Calling：模型原生 API 参数 vs Prompt 注入
- [ ] 两段式推理：意图识别 → 工具执行 → 结果综合
- [ ] ReAct：Thought → Action → Observation 循环
- [ ] ONNX：跨框架模型格式 + 推理引擎
- [ ] MobileNetV2：轻量级图像分类，3.4M 参数
- [ ] ASR 自建：whisper.cpp | TTS 自建：sherpa-onnx
