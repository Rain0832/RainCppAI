# v1.2.0 优化小结：线程安全升级 & LRU 内存管理

> 对应 TODO 优化 3 · 中等改动

---

## 一、问题背景

ChatServer 中有多个共享数据结构被多个 IO 线程并发访问：

```cpp
std::unordered_map<int, std::unordered_map<std::string, shared_ptr<AIHelper>>> chatInformation;
std::unordered_map<int, bool> onlineUsers_;
std::unordered_map<int, vector<string>> sessionsIdsMap;
std::unordered_map<int, shared_ptr<ImageRecognizer>> ImageRecognizerMap;
```

原实现使用 `std::mutex` 独占锁，所有读写操作串行化。但实际场景中：
- **读多写少**：大部分请求是查找已有会话（读），创建新会话（写）频率低
- **锁粒度过大**：一个 mutex 保护整个 map，不同用户的请求也互相阻塞
- **无内存上限**：会话只增不减，长期运行会 OOM

## 二、解决方案

### 2.1 std::mutex → std::shared_mutex（读写锁）

```cpp
// Before
std::mutex mutexForChatInformation;
std::lock_guard<std::mutex> lock(mutexForChatInformation); // 所有操作独占

// After
mutable std::shared_mutex rwMutexForChatInfo;
std::shared_lock<std::shared_mutex> rlock(rwMutexForChatInfo);  // 读操作：共享锁
std::unique_lock<std::shared_mutex> wlock(rwMutexForChatInfo);  // 写操作：独占锁
```

**读写分离模式**：先用 `shared_lock` 尝试查找，找到则直接使用；找不到再升级为 `unique_lock` 创建。

```cpp
// 先读锁查找
std::shared_lock<std::shared_mutex> rlock(rwMutexForChatInfo);
auto it = chatInformation.find(userId);
if (found) { AIHelperPtr = it->second[sessionId]; }
rlock.unlock();

// 若未找到，写锁创建
if (!AIHelperPtr) {
    std::unique_lock<std::shared_mutex> wlock(rwMutexForChatInfo);
    // double-check 后创建
    chatInformation[userId].emplace(sessionId, make_shared<AIHelper>());
}
```

### 2.2 LRU 淘汰策略

基于 `std::list` + `unordered_map` 的经典 O(1) LRU：

```
list: [newest] <-> ... <-> [oldest]
map:  key -> list::iterator

touch(key): 移到 list 头部
evict():    删除 list 尾部（最久未访问的会话）
```

```cpp
static constexpr size_t MAX_SESSIONS = 500;
std::list<std::string> lruList_;                          // 访问顺序
std::unordered_map<std::string, list<string>::iterator> lruMap_;  // O(1) 定位

void touchSession(int userId, const string& sessionId) {
    string key = to_string(userId) + ":" + sessionId;
    if (lruMap_.count(key)) lruList_.erase(lruMap_[key]);
    lruList_.push_front(key);
    lruMap_[key] = lruList_.begin();
}

void evictIfNeeded() {
    while (lruList_.size() > MAX_SESSIONS) {
        // 淘汰最久未访问的会话，释放 AIHelper 内存
        string oldest = lruList_.back();
        lruList_.pop_back();
        lruMap_.erase(oldest);
        // 从 chatInformation 中删除对应 session
    }
}
```

## 三、涉及文件

| 文件 | 改动 |
|------|------|
| `ChatServer.h` | mutex → shared_mutex, 新增 LRU 数据结构和方法 |
| `ChatServer.cpp` | 新增 touchSession / evictIfNeeded 实现 |
| `ChatSendHandler.cpp` | 读写锁分离 + LRU touch |
| `ChatCreateAndSendHandler.cpp` | 写锁 + LRU touch |
| `ChatHistoryHandler.cpp` | 读写锁分离 |
| `ChatSessionsHandler.cpp` | 读锁 |
| `ChatLoginHandler.cpp` | shared_mutex 写锁 |
| `ChatLogoutHandler.cpp` | shared_mutex 写锁 |
| `AIUploadSendHandler.cpp` | ImageRecognizer 读写锁分离 |

## 四、面试考点

- **为什么用 shared_mutex 而不是 mutex？** 读多写少场景，shared_lock 允许多个读者并行，throughput 更高
- **LRU 为什么用 list + map？** list 提供 O(1) 的移动/删除（splice/erase），map 提供 O(1) 查找
- **为什么 shared_mutex 声明为 mutable？** const 成员函数中也可能需要获取读锁
- **double-check 为什么重要？** 读锁释放到写锁获取之间可能有其他线程已创建了该 session
