# 部署指南

## 生产环境 Checklist

- [ ] 数据库已初始化（8 张表 + 外键）
- [ ] `config.json` 已配置（DB / LLM API Key / JWT Secret）
- [ ] ONNX Runtime 已安装，模型已下载（可选 — 用于视觉识别）
- [ ] Nginx 反向代理已配置
- [ ] Systemd 服务已注册
- [ ] SSL 证书已配置（生产环境必须 HTTPS）

## Nginx 反向代理配置

完整配置文件：[`deploy/nginx.conf`](https://github.com/Rain0832/RainCppAI/blob/main/deploy/nginx.conf)

### 安装步骤

```bash
# 1. 复制配置
sudo cp deploy/nginx.conf /etc/nginx/sites-available/raincppai

# 2. 启用站点
sudo ln -s /etc/nginx/sites-available/raincppai /etc/nginx/sites-enabled/

# 3. 测试并重载
sudo nginx -t && sudo systemctl reload nginx
```

### 关键配置项

```nginx
# SSE 流式响应 —— proxy_buffering 必须关闭！
location / {
    proxy_pass http://raincppai_backend;
    proxy_buffering off;   # ← 关键：SSE 依赖此配置
    proxy_cache off;
    proxy_http_version 1.1;
    proxy_set_header Connection "upgrade";
}

# 静态资源缓存
location /web/ {
    root /opt/raincppai;
    expires 7d;
    add_header Cache-Control "public, immutable";
}
```

### 启用 HTTPS

```nginx
# 取消注释以下行：
# listen 443 ssl http2;
# ssl_certificate     /etc/ssl/certs/raincppai.crt;
# ssl_certificate_key /etc/ssl/private/raincppai.key;

# 建议使用 Let's Encrypt 免费证书：
sudo certbot --nginx -d your-domain.com
```

## Systemd 服务

配置：[`deploy/raincppai.service`](https://github.com/Rain0832/RainCppAI/blob/main/deploy/raincppai.service)

```bash
# 安装
sudo cp deploy/raincppai.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable raincppai
sudo systemctl start raincppai

# 常用命令
sudo systemctl status raincppai   # 查看状态
sudo journalctl -u raincppai -f   # 实时日志
sudo systemctl restart raincppai  # 重启
```

## 环境变量对照

所有 `config.json` 值均可通过环境变量覆盖（ConfigManager 自动加载）：

| config.json 路径 | 环境变量 |
|-----------------|---------|
| `db.host` | `DB_HOST` |
| `db.port` | `DB_PORT` |
| `db.user` | `DB_USER` |
| `db.password` | `DB_PASSWORD` |
| `db.name` | `DB_NAME` |
| `mail.username` | `MAIL_USERNAME` |
| `mail.password` | `MAIL_PASSWORD` |
| `jwt.secret` | `JWT_SECRET` |
| `default_api_keys.dashscope` | `DEFAULT_API_KEYS_DASHSCOPE` |
| `default_api_keys.doubao` | `DEFAULT_API_KEYS_DOUBAO` |

```bash
# 生产环境示例（不需要 config.json 中的敏感字段）
export DB_PASSWORD="xxx"
export JWT_SECRET="$(openssl rand -hex 32)"
export DEFAULT_API_KEYS_DASHSCOPE="sk-xxx"
./http_server -p 8080
```

## 目录结构（生产环境）

```
/opt/raincppai/
├── http_server         # C++ 编译产物
├── config.json         # 运行时配置（不要提交到 Git！）
├── models.json         # 模型注册表
├── web/                # 前端静态资源
├── mcp_servers/        # Python MCP 微服务
├── logs/               # 运行时日志
└── uploads/            # 用户上传图片
```
