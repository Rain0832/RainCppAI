# Storage — 模块技术文档

## 模块职责

Storage 是项目的**持久化基础设施层**，从 HttpServer 中剥离的独立顶层模块。负责 MySQL 数据库连接池管理、Prepared Statement 参数绑定、SQL 查询/更新统一封装。将来可扩展端侧记忆、向量存储等持久化能力。

## 核心文件流转逻辑

```
MysqlUtil::init(host, user, pwd, db, poolSize)
  → DbConnectionPool::getInstance().init(...)
    → createConnection() × poolSize
      → DbConnection(host, user, pwd, db) × N

MysqlUtil::executeQuery(sql, args...)
  → DbConnectionPool::getConnection()
    → DbConnection::executeQuery(sql, args...)
      → conn_->prepareStatement(sql)
      → bindParams(stmt, 1, args...)  // 模板递归，类型安全
      → stmt->executeQuery()

MysqlUtil::executeUpdate(sql, args...)
  → ...同上 → stmt->executeUpdate()
```

## 关键文件

| 文件 | 职责 |
|------|------|
| `include/storage/DbConnection.h` | 单个连接封装，Prepared Statement 模板方法（DML）+ `executeRawSql()` 文本协议（DDL），多类型参数绑定 |
| `include/storage/DbConnectionPool.h` | 连接池单例，超时等待、自动重连、心跳检测 |
| `include/storage/MysqlUtil.h` | 对外门面类，静态 init + 模板 executeQuery/executeUpdate |
| `include/storage/DbException.h` | 数据库异常类型 |

## bindParams 类型支持

| 类型 | 绑定方法 | 用途 |
|------|---------|------|
| `std::string` | `setString` | 文本字段、session ID、content |
| `const char*` | `setString` | C 字符串字面量 |
| `int` | `setInt` | 用户 ID 等 |
| `long long` | `setInt64` | 时间戳、BIGINT 字段 |
| `bool` | `setBoolean` | is_deleted TINYINT(1) |
| `std::nullptr_t` | `setNull` | payload JSON NULL |

## 对外依赖与耦合边界

## Plan 1 变更摘要 (v2.2.13)
- 数据库全量重设计（8 表 + FOREIGN KEY + 1NF/2NF/3NF）
- DDL 见 `AIServerCore/src/server/ChatServer.cpp::initDatabase()`
|------|------|
| MySQL Connector C++ 8 | `cppconn/` 头文件 |

### 被依赖

- AIServerCore: `ChatServer::mysqlUtil_` 成员，所有 Handler 通过临时实例或注入使用
- AIEngine: `AIHelper::mysqlUtil_` 指针（通过 ChatServer 注入）

### 命名空间

- `storage::` — 存储层命名空间

## Database Schema (v3.0.0)

8 tables with FOREIGN KEY constraints, 1NF/2NF/3NF compliant:

| Table | Key Columns | FK |
|-------|-------------|-----|
| accounts | id, username UNIQUE, password_hash, email, role ENUM(user/admin/org), is_disabled, failed_attempts, locked_until, invite_code_id | — |
| sessions | id VARCHAR(64) PK, account_id, title, is_deleted | account_id → accounts.id |
| messages | id, session_id, role ENUM(user/assistant/system/tool), content, model, tool_call_id, payload JSON | session_id → sessions.id CASCADE |
| api_keys | id, account_id, provider, api_key | account_id → accounts.id |
| invite_codes | id, code UNIQUE, created_by, max_uses, used_count, expires_at, is_disabled, is_admin | created_by → accounts.id |
| verification_codes | id, email, code, purpose ENUM(register/reset_password), is_used, expires_at | — |
| feedback | id, account_id, content TEXT | account_id → accounts.id |
| call_logs | id, session_id, account_id, model, provider, duration_ms, status ENUM(success/error/timeout) | — |

> v3.0.0 新增字段: accounts.invite_code_id / failed_attempts / locked_until, invite_codes.is_admin

See AIServerCore/src/server/ChatServer.cpp::initDatabase() for DDL.
