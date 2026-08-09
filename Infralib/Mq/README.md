# Infralib/Mq — RabbitMQ 消息队列层

> v3.2.0 重新激活：用于多模态任务异步削峰。

## 文件

| 文件 | 职责 | 状态 |
|------|------|------|
| `TaskMessage.h` | JSON 消息协议（version/taskId/type/payload/replyTo/ttl） | ✅ v3.2.0 新增 |
| `TaskProducer.h/.cpp` | AMQP-CPP 生产者：publish() 投递任务，不阻塞 Reactor | ✅ v3.2.0 新增 |
| `TaskConsumer.h/.cpp` | 独立消费者进程：type dispatch（vision/tts/summarize），结果写 Redis | ✅ v3.2.0 新增 |
| `MQManager.h/.cpp` | RabbitMQ 连接池（旧版） | ⚠️ v2.2.0 退役，仅存档 |
| `README.md` | 本文件 | ✅ v3.2.0 更新 |

## 架构

```
HTTP Thread (Reactor)
  └─ ChatSseHandler (image detected)
       └─ TaskProducer::publish(msg)
            └─ AMQP::Channel::publish(exchange, routingKey, body)
       └─ return 202 + {"taskId":"sn_xxx"}

Worker Process (独立进程)
  └─ TaskConsumer
       └─ AMQP::consume(queue)
            └─ type dispatch → process → Redis SET task:{id}

Frontend
  └─ SSE poll GET /task/{id}/status → Redis GET → result
```

## 代码对应
- `AIServerCore/src/controller/ChatSseHandler.cpp`: 视觉请求检测、TaskMessage 组装与 `TaskProducer::publish()` 投递
- `AIServerCore/src/server/ChatServer.cpp`: 初始化 RabbitMQ 连接并声明 `vision_tasks` / `tts_tasks`
- `AIServerCore/src/controller/TaskStatusHandler.cpp`: 根据 taskId 从 Redis 读取结果并返回处理状态
- `Infralib/Mq/TaskProducer.{h,cpp}`: AMQP-CPP 生产者封装，负责推送异步任务消息
- `Infralib/Mq/TaskConsumer.{h,cpp}`: 独立消费者进程，拉取队列任务并将结果写入 Redis

## 消息协议

```json
{
  "version": "1",
  "taskId": "sn_20260808_a1b2c3d4",
  "type": "vision|tts|summarize",
  "createdAt": 1691481600000,
  "payload": { "userId": 42, "sessionId": "sn_xxx", "imageBase64": "..." },
  "replyTo": "task_result_queue",
  "ttl": 60000
}
```

## 历史

| 版本 | 状态 |
|------|------|
| v1.x | MQManager 负责异步 DB 写入 (SimpleAmqpClient + rabbitmq-c) |
| v2.2.0 | 退役：DB 写入切换为同步 Prepared Statement |
| v3.2.0 | 重新激活：AMQP-CPP 异步任务削峰（多模态/图片/摘要） |
