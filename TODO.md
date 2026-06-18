# RainCppAI TODO

> 四象限优先级：**P0** 致命 → **P1** 高优 → **P2** 中期 → **P3** 远期

---

## 🔴 P0 — 安全 / 稳定性

- [x] **SQL 注入修复**：`pushMessageToMysql` SQL 拼接 → Prepared Statement（v2.2.0 全局完成）
- [ ] **Agent Loop 上下文持久化**：tool 消息不丢失，重启后可恢复
- [ ] **连接池优雅关闭**：`detach()` 线程 → 统一生命周期管理
- [ ] **chatInformation 全局锁拆分为分片锁**

---

## 🟠 P1 — 功能完善

- [ ] **天气工具替换**：`get_weather` OpenWeather → 和风天气国内 API
- [x] **AliyunMcpStrategy 与 AliyunStrategy 去重**：已合并为 AliyunStrategy（Commit 7）
- [ ] **ASR 语音识别**：前端录音 → 后端转发 → 百度 ASR API
- [ ] **TTS 多供应商**：百度 / Edge-TTS / 阿里云 可选
- [x] **前端流式统一**：`/chat/send`、`/chat/send-new-session` 迁移至 `/chat/send-stream`，下线 `AIHelper::chat()` 非流式路径（`ChatSendHandler` / `ChatCreateAndSendHandler`）
- [x] **前端工程化解耦**：HTML/CSS/JS 独立文件拆分 + StaticFileHandler 通用静态文件服务 + MIME 类型映射
- [x] **`user_api_keys` 表服务端读取**：`GET /api/user/apikey` 返回掩码列表（v2.2.0 完成）

---

## 🔵 P2 — 架构演进

- [ ] **AIServerCore 三层拆分**：Controller → Service → Repository（Storage 模块已完成 DB 层解耦）
- [ ] **AIEngine 独立编译为静态库**（`libaiengine.a`）
- [ ] **结构化日志**：JSON 格式 + request_id 链路追踪
- [ ] **Prometheus 指标**：QPS / 延迟 / 连接池利用率
- [ ] **HTTP/2 支持**（muduo 升级或替换）

---

## ⚪ P3 — 远期规划

- [ ] **前端框架迁移**：Vue 3 / React SPA（`web/app/`）
- [ ] **WebSocket 支持**：替代 SSE 轮询
- [ ] **插件系统**：Handler / Strategy 动态加载
- [ ] **Kubernetes 部署**：Helm Chart + 水平伸缩
- [ ] **多语言 SDK**：Python / Node.js SDK
- [ ] **RBAC 权限系统**：引入账号体系，区分 User 级别和 Admin 级别。Admin 可访问管理后台，User 仅能使用聊天功能
- [ ] **Admin 动态看板**：提供管理后台，允许 Admin 动态配置和增删"厂商-模型"注册表（`provider` / `model`），取代 `ModelListHandler` 中的静态 JSON。前端模型下拉框所见即 Admin 所配
- [ ] **角色扩展**：`Message` 枚举增加 `System` 设定词和 `Tool` 结果类型，支持持久化存储到 `messages` 表
