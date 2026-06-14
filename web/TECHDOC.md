# web/ — 模块技术文档

## 模块职责

`web/` 是项目的**前端资源目录**，包含服务端 HTML 模板（由 ChatServer 读取并注入数据返回）以及 CSS / JS / 静态资源。v2.1.0 已将 AI.html 大单体拆分为独立的 HTML/CSS/JS 工程化结构，并新增 `StaticFileHandler` 支持 MIME 类型的通用静态文件服务。

## 目录结构与流转逻辑

```
浏览器请求
  └─► HttpServer Router
        ├─► GET / → ChatEntryHandler → 读取 web/entry.html → 返回
        ├─► GET /menu → AIMenuHandler → 读取 web/menu.html → 返回
        ├─► GET /chat → ChatHandler → 读取 web/AI.html → 返回（注入 userId script）
        ├─► GET /css/:file → StaticFileHandler → 读取 web/css/:file → 返回（Content-Type: text/css）
        ├─► GET /js/:file → StaticFileHandler → 读取 web/js/:file → 返回（Content-Type: application/javascript）
        ├─► GET /assets/:path → StaticFileHandler → 读取 web/assets/:path → 返回
        └─► GET /upload → AIUploadHandler → 读取 web/upload.html → 返回

前端模块加载链（AI.html）
  └─► <link rel="stylesheet" href="/css/style.css">   ← 独立样式表（CSS 变量 + 暗色主题）
  └─► <script src="CDN: marked.min.js + purify.min.js">   ← 第三方渲染库
  └─► <script type="module" src="/js/ui.js">            ← ES Module 入口
        └─► import { ... } from './api.js'              ← 网络层导入

SSE 流式（前端 JS）
  └─► fetch POST /chat/send-stream
        ├─ body.getReader() 逐 token 渲染
        ├─ sessionId 为空时 → 后端自动创建并 SSE 回传
        └─ 静默过滤 Tool Call 碎片（仅渲染纯文本 token）
```

### 当前文件

| 文件 | 说明 |
|------|------|
| `entry.html` | 登录 / 注册页 |
| `menu.html` | 功能菜单（聊天 / 上传 / 个人中心） |
| `AI.html` | AI 聊天主界面（v2.1.0 已拆分为 CSS/JS 独立文件，仅保留 DOM 骨架） |
| `upload.html` | 图像识别上传页 |
| `NotFound.html` | 404 错误页 |
| `config.json` | 前端配置（模型列表、UI 设置） |
| `css/style.css` | 聊天页样式表（CSS 变量 + 明暗主题 + 响应式适配） |
| `js/api.js` | 网络 I/O 模块（SSE 流式、TTS、会话管理、历史记录） |
| `js/ui.js` | UI 渲染模块（消息气泡、打字机效果、主题切换、事件绑定） |

### 目录布局

```
web/
├── *.html            ← 服务端模板（C++ 读取 + 注入）
├── config.json       ← 前端配置
├── css/
│   └── style.css     ← AI 聊天页样式
├── js/
│   ├── api.js        ← 网络请求层（ES Module 导出）
│   └── ui.js         ← UI 交互层（ES Module 导入 api.js）
├── assets/
│   ├── images/       ← 图片、图标
│   └── fonts/        ← 字体
└── TECHDOC.md
```

## 对外依赖与耦合边界

### 依赖

| 依赖 | 说明 |
|------|------|
| HttpServer | HTML 模板由 Handlers 读取并发送 |
| AIServerCore | Handler 确定路由 → 文件映射；StaticFileHandler 服务 CSS/JS/静态资源 |

### 被依赖

- **HttpServer / AIServerCore**：运行时读取 `web/*.html`、`web/config.json`、`web/css/*.css`、`web/js/*.js`
- **无编译期依赖**（纯静态资源）

### 命名空间

- 不适用（前端资源无 C++ 命名空间概念）

## v2.1.0 变更摘要

| 变更项 | 说明 |
|------|------|
| HTML 拆分 | 882 行大单体 AI.html → 56 行 DOM 骨架 + css/style.css + js/api.js + js/ui.js |
| JS 模块化 | ES Module 导入/导出，api.js 负责网络 I/O，ui.js 负责 DOM 渲染 |
| 流式统一 | 新建会话和已有会话统一走 SSE `POST /chat/send-stream`，后端自动生成 sessionId |
| StaticFileHandler | 新增 C++ 通用静态文件服务，支持 MIME 类型映射（text/css, application/javascript 等） |
| 非流式移除 | `fetch('/chat/send')` 和 `fetch('/chat/send-new-session')` 已从 JS 中删除 |