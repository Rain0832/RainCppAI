# API 参考

> 完整 HTTP API 路由表（v3.0.0）

## 公开接口（无需认证）

| Method | Path | Description |
|--------|------|-------------|
| GET | `/` `/entry` | Login / Register page |
| GET | `/register` | Registration page |
| POST | `/login` | Login |
| POST | `/register` | Register |
| POST | `/api/invite/verify` | Verify invite code |
| POST | `/api/verify/send` | Send email verification code |
| POST | `/api/verify/check` | Check email verification code |
| POST | `/mcp` | MCP JSON-RPC 2.0 |
| GET | `/health` | Health check (MySQL `SELECT 1`) |
| GET | `/metrics` | Prometheus metrics |

## 用户接口（需 JWT 认证）

| Method | Path | Description |
|--------|------|-------------|
| GET | `/chat` | Chat page |
| POST | `/chat/send-stream` | **SSE streaming dialog** (only dialog entry) |
| GET | `/chat/sessions` | Session list |
| POST | `/chat/history` | Session history |
| POST | `/chat/delete-session` | Soft-delete session |
| POST | `/chat/update-title` | Update session title |
| POST | `/chat/tts` | Text-to-speech |
| POST | `/user/logout` | Logout |
| GET | `/upload` | Image recognition page |
| POST | `/upload/send` | Image recognition upload |
| GET/POST | `/api/user/apikey` | API Key CRUD |
| POST | `/api/user/password` | Change password |
| POST | `/api/feedback` | Submit feedback |
| GET | `/api/chat/models` | Model registry (from `models.json`) |

## Admin 接口（需 Admin 角色）

| Method | Path | Description |
|--------|------|-------------|
| GET | `/admin/dashboard` | Admin panel |
| GET | `/admin/sse` | Real-time dashboard SSE |
| GET | `/admin/logs` | Log viewer |
| GET | `/admin/api/users` | User list |
| POST | `/admin/api/users/toggle` | Enable/disable user |
| GET | `/admin/api/feedback` | Feedback list |
| GET | `/admin/api/invite-codes` | Invite code list |
| POST | `/admin/api/invite-codes/create` | Create invite code |
| POST | `/admin/api/invite-codes/toggle` | Enable/disable invite code |

## 认证方式

所有需要认证的接口通过 **HttpOnly Cookie** 携带 JWT Token（Cookie 名：`raincpp_token`）。

前端 `fetch` 请求必须设置 `credentials: 'include'`。

```javascript
fetch('/chat/send-stream', {
    method: 'POST',
    credentials: 'include',  // ← 必须
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({...})
});
```
