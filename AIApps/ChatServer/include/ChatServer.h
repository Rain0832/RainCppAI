/**
 * @file ChatServer.h
 * @brief AI聊天服务器头文件
 * 
 * 本文件定义了ChatServer类，是整个AI聊天应用的核心服务器接口。
 * 主要功能包括：
 * - HTTP服务器初始化和配置
 * - 数据库连接和消息持久化
 * - 会话管理和用户状态维护
 * - HTTP路由映射和请求分发
 * - 中间件集成（如CORS支持）
 * - 响应封装和错误处理
 * 
 * 架构设计思路：
 * 1. 采用分层架构：网络层 → 路由层 → 业务逻辑层 → 数据访问层
 * 2. 基于muduo网络库提供高性能HTTP服务
 * 3. 使用策略模式处理不同类型的AI请求
 * 4. 支持会话管理实现多用户并发访问
 */

#pragma once

#include <atomic>
#include <memory>
#include <tuple>
#include <unordered_map>
#include <mutex>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <string>
#include <vector>


#include "../../../HttpServer/include/http/HttpServer.h"
#include "../../../HttpServer/include/utils/MysqlUtil.h"
#include "../../../HttpServer/include/utils/FileUtil.h"
#include "../../../HttpServer/include/utils/JsonUtil.h"
#include"AIUtil/AISpeechProcessor.h"
#include"AIUtil/AIHelper.h"
#include"AIUtil/ImageRecognizer.h"
#include"AIUtil/base64.h"
#include"AIUtil/MQManager.h"


class ChatLoginHandler;
class ChatRegisterHandler;
class ChatLogoutHandler;
class ChatHandler;
class ChatEntryHandler;
class ChatSendHandler;
class ChatHistoryHandler;

class AIMenuHandler;
class AIUploadHandler;
class AIUploadSendHandler;


class ChatCreateAndSendHandler;
class ChatSessionsHandler;
class ChatSpeechHandler;

/**
 * @brief AI聊天服务器核心类
 * 
 * 负责管理整个聊天应用的服务器端逻辑，包括用户认证、消息处理、会话管理等
 */
class ChatServer {
public:
	/**
	 * @brief ChatServer构造函数
	 * 
	 * 初始化HTTP服务器并设置基本参数，采用依赖注入方式配置muduo网络库
	 * 
	 * @param port 服务器监听端口
	 * @param name 服务器名称（用于日志标识）
	 * @param option muduo网络库配置选项
	 */
	ChatServer(int port,
		const std::string& name,
		muduo::net::TcpServer::Option option = muduo::net::TcpServer::kNoReusePort);

	/**
	 * @brief 设置服务器工作线程数
	 * 
	 * 配置muduo网络库的IO线程池大小，影响并发处理能力
	 * 
	 * @param numThreads 线程数量（通常设置为CPU核心数的1-2倍）
	 */
	void setThreadNum(int numThreads);
	
	/**
	 * @brief 启动HTTP服务器
	 * 
	 * 调用底层HttpServer的启动方法，开始监听端口和处理请求
	 */
	void start();
	
	/**
	 * @brief 初始化聊天消息数据
	 * 
	 * 服务器启动时从MySQL数据库加载历史聊天记录
	 * 实现消息持久化和会话恢复功能
	 */
	void initChatMessage();
	
private:
	friend class ChatLoginHandler;
	friend class ChatRegisterHandler;
	friend  ChatLogoutHandler;
	friend class ChatHandler;
	friend class ChatEntryHandler;
	friend class ChatSendHandler;
	friend class AIMenuHandler;
	friend class AIUploadHandler;
	friend class AIUploadSendHandler;
	friend class ChatHistoryHandler;

	friend class ChatCreateAndSendHandler;
	friend class ChatSessionsHandler;
	friend class ChatSpeechHandler;

private:
	/**
	 * @brief 服务器初始化入口方法
	 * 
	 * 按照依赖关系顺序初始化各个组件：
	 * 1. 数据库连接 → 2. 会话管理 → 3. 中间件 → 4. 路由配置
	 * 这种顺序确保了底层服务先于上层业务初始化
	 */
	void initialize();
	
	/**
	 * @brief 初始化会话管理系统
	 * 
	 * 配置基于内存的会话存储，支持用户登录状态管理
	 * 采用SessionManager统一管理会话生命周期
	 */
	void initializeSession();
	
	/**
	 * @brief 初始化HTTP路由映射
	 * 
	 * 注册所有API端点和对应的处理器类，采用RESTful风格设计：
	 * - GET请求用于获取资源
	 * - POST请求用于创建或修改资源
	 * 
	 * 路由设计原则：
	 * 1. 按功能模块分组（chat、upload、user等）
	 * 2. 使用动词+名词的命名规范
	 * 3. 支持路径参数和查询参数
	 */
	void initializeRouter();
	
	/**
	 * @brief 初始化HTTP中间件
	 * 
	 * 配置请求处理管道，支持跨域资源共享(CORS)等通用功能
	 * 中间件在请求到达处理器之前执行预处理
	 */
	void initializeMiddleware();
	
	/**
	 * @brief 从MySQL数据库读取聊天消息
	 * 
	 * 执行SQL查询获取所有用户的聊天记录，按时间顺序加载
	 * 构建用户会话树结构，恢复AI助手状态
	 */
	void readDataFromMySQL();

	/**
	 * @brief 统一响应封装方法
	 * 
	 * 标准化HTTP响应格式，确保所有接口返回一致的数据结构
	 * 提供错误处理和日志记录功能
	 * 
	 * @param version HTTP协议版本（如"HTTP/1.1"）
	 * @param statusCode HTTP状态码（200、404、500等）
	 * @param statusMsg 状态描述信息
	 * @param close 是否关闭连接（长连接/短连接）
	 * @param contentType 响应内容类型（如"application/json"）
	 * @param contentLen 内容长度
	 * @param body 响应体数据（JSON格式）
	 * @param resp HttpResponse指针，用于设置响应参数
	 */
	void packageResp(const std::string& version, http::HttpResponse::HttpStatusCode statusCode,
		const std::string& statusMsg, bool close, const std::string& contentType,
		int contentLen, const std::string& body, http::HttpResponse* resp);

	/**
	 * @brief 设置会话管理器
	 * 
	 * 配置会话管理组件，支持用户登录状态维护
	 * 
	 * @param manager 会话管理器实例
	 */
	void setSessionManager(std::unique_ptr<http::session::SessionManager> manager)
	{
		httpServer_.setSessionManager(std::move(manager));
	}
	
	/**
	 * @brief 获取会话管理器
	 * 
	 * @return 当前会话管理器实例指针
	 */
	http::session::SessionManager* getSessionManager() const
	{
		return httpServer_.getSessionManager();
	}

	http::HttpServer	httpServer_;           ///< HTTP服务器实例

	http::MysqlUtil		mysqlUtil_;            ///< MySQL数据库工具实例

	std::unordered_map<int, bool>	onlineUsers_;      ///< 在线用户状态映射
	std::mutex	mutexForOnlineUsers_;                  ///< 在线用户状态锁

	/**
	 * @brief 聊天信息存储结构
	 * 
	 * 外层键：用户ID
	 * 内层键：会话ID
	 * 值：AI助手实例
	 */
	std::unordered_map<int, std::unordered_map<std::string,std::shared_ptr<AIHelper> > > chatInformation;
	std::mutex	mutexForChatInformation;                ///< 聊天信息锁

	std::unordered_map<int, std::shared_ptr<ImageRecognizer> > ImageRecognizerMap;  ///< 图像识别器映射
	std::mutex	mutexForImageRecognizerMap;             ///< 图像识别器锁

	std::unordered_map<int,std::vector<std::string> > sessionsIdsMap;  ///< 用户会话ID列表映射
	std::mutex mutexForSessionsId;                                     ///< 会话ID列表锁

};
