// 从 AIEngine/include/common/MQManager.h 迁移
// RabbitMQ 异步写入已在 v2.2.0 中移除，替换为同步 Prepared Statement。
// 此文件保留以供将来参考，可能用于与消息队列系统的集成。
// CMake 不会链接此文件——仅保留存档。
// 当 RabbitMQ 基础设施完全拆除或重新设计时，请删除。
#pragma once

#include <string>

// (原始内容省略——应使用 git log 查找旧版本或使用磁盘上的 AIEngine/include/common/MQManager.h)
