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

### 依赖

| 依赖 | 说明 |
|------|------|
| MySQL Connector C++ 8 | `cppconn/` 头文件 |

### 被依赖

- AIServerCore: `ChatServer::mysqlUtil_` 成员，所有 Handler 通过临时实例或注入使用
- AIEngine: `AIHelper::mysqlUtil_` 指针（通过 ChatServer 注入）

### 命名空间

- `storage::` — 存储层命名空间