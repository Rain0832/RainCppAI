# web/ — 模块技术文档

## 模块职责

`web/` 是项目的**前端资源目录**，包含服务端 HTML 模板（由 ChatServer 读取并注入数据返回）以及 CSS / JS / 静态资源。v2.1.0 已将 AI.html 大单体拆分为独立的 HTML/CSS/JS 工程化结构，并新增 `StaticFileHandler` 支持 MIME 类型的通用静态文件服务。

## 目录结构与流转逻辑

```
浏览器请求
  └─► HttpServer Router
        ├─► GET / → ChatEntryHandler → 读取 web/entry.html → 返回
        ├─► GET /register → ChatEntryHandler → 读取 web/register.html → 返回
        ├─► GET /chat → ChatHandler → 读取 web/AI.html → 返回
        ├─► GET /css/:file → StaticFileHandler → 读取 web/css/:file → 返回
        ├─► GET /js/:file → StaticFileHandler → 读取 web/js/:file → 返回
        ├─► GET /assets/:path → StaticFileHandler → 读取 web/assets/:path → 返回
        └─► GET /admin/dashboard → AdminDashboardHandler → 读取 web/admin/dashboard.html → 返回

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
| `entry.html` | 登录 / 注册页（双主题背景图） |
| `register.html` | 多步注册页（邀请码 → 邮箱验证 → 注册） |
| `AI.html` | AI 聊天主界面（视觉上传 + SSE 流式） |
| `upload.html` | 独立图像识别页 |
| `NotFound.html` | 404 错误页 |
| `admin/dashboard.html` | Admin 管理后台（看板/用户/邀请码/反馈/日志/修改密码） |
| `admin/logs.html` | 日志查看器 |
| `css/style.css` | 聊天页样式（CSS 变量 + 明暗主题 + 背景图） |
| `css/entry.css` | 登录/注册页样式（hero 品牌宣导 + 背景图） |
| `js/api.js` | 网络 I/O 模块（SSE/API/会话/历史） |
| `js/ui.js` | UI 渲染模块（消息气泡/主题/个人中心/修改密码/Vision 上传） |
| `js/entry.js` | 登录/注册交互逻辑 |
| `js/register.js` | 多步注册流程 JS |
| `assets/images/` | 图标 + 主题背景图 |

### 目录布局

```
web/
├── *.html            ← 服务端模板
├── js/
│   ├── api.js        ← 网络请求层
│   ├── ui.js         ← UI 交互层
│   ├── entry.js      ← 登录页 JS
│   └── register.js   ← 注册页 JS
├── css/
│   ├── style.css     ← 聊天页样式
│   └── entry.css     ← 登录/注册页样式
├── assets/
│   └── images/       ← 图标、背景图
├── admin/
│   ├── dashboard.html ← Admin 后台
│   └── logs.html      ← 日志查看器
└── TECHDOC.md
```

## v3.0.0 变更摘要
- Vision-Agent: 📎 附件上传 + Base64 编码 + 缩略预览
- 主题背景图: entry.html + AI.html 双主题动态背景
- Admin 邀请码面板: 生成/查看/启禁
- 修改密码: 个人中心 + Admin 双端 UI
- 废弃路由清理: /menu 路由已移除（v2.9.6），menu.html 仍存盘待清理

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