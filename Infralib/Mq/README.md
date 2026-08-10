# Infralib/Mq — 已退役：RabbitMQ 消息队列层

存放 `MQManager.h` 和 `MQManager.cpp`，这些文件最初负责通过 RabbitMQ 进行异步数据库写入。

**状态**：自 v2.2.0 起已退役，当时数据库写入切换为同步 `Prepared Statement`。这些文件已从 CMake 构建中排除，仅为历史参考而保留。当 RabbitMQ 基础设施完全拆除或重新设计时，可以安全删除整个 `Infralib/Mq/` 目录。

## 文件

- `MQManager.h` — RabbitMQ 连接池和消费者线程池包装器
- `MQManager.cpp` — 实现
- `README.md` — 本文件
