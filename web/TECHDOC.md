# web/ — 模块技术文档

## 模块职责

`web/` 是项目的**前端资源目录**，包含服务端 HTML 模板（由 ChatServer 读取并注入数据返回）以及 CSS / JS / 静态资源。

## 目录结构与流转逻辑

```
浏览器请求
  └─► HttpServer Router
        ├─► GET / → ChatEntryHandler → 读取 web/entry.html → 返回
        ├─► GET /menu → AIMenuHandler → 读取 web/menu.html → 返回
        ├─► GET /chat → ChatHandler → 读取 web/AI.html → 返回
        └─► GET /upload → AIUploadHandler → 读取 web/upload.html → 返回

SSE 流式（前端 JS）
  └─► fetch POST /chat/send-stream
        └─► body.getReader() 逐 token 渲染
```

### 当前文件

| 文件 | 说明 |
|------|------|
| `entry.html` | 登录 / 注册页 |
| `menu.html` | 功能菜单（聊天 / 上传 / 个人中心） |
| `AI.html` | AI 聊天主界面（含 SSE 前端逻辑） |
| `upload.html` | 图像识别上传页 |
| `NotFound.html` | 404 错误页 |
| `config.json` | 前端配置（模型列表、UI 设置） |

### 目录布局

```
web/
├── *.html            ← 服务端模板（C++ 读取 + 注入）
├── config.json       ← 前端配置
├── css/              ← 样式表
├── js/               ← 原生 JS 脚本
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
| AIServerCore | Handler 确定路由 → 文件映射 |

### 被依赖

- **HttpServer / AIServerCore**：运行时读取 `web/*.html`、`web/config.json`
- **无编译期依赖**（纯静态资源）

### 命名空间

- 不适用（前端资源无 C++ 命名空间概念）
