#pragma once
#include "SslContext.h"
#include <muduo/net/TcpConnection.h>
#include <muduo/net/Buffer.h>
#include <muduo/base/noncopyable.h>
#include <openssl/ssl.h>
#include <memory>

namespace ssl 
{

/**
 * SSL连接消息回调函数类型定义
 * 用于处理解密后的应用层数据
 */
using MessageCallback = std::function<void(const std::shared_ptr<muduo::net::TcpConnection>&,
                                         muduo::net::Buffer*,
                                         muduo::Timestamp)>;

/**
 * SSL连接封装类
 * 负责SSL/TLS协议的握手、加密传输和解密处理
 * 集成muduo网络库，提供透明的SSL加密通信能力
 */
class SslConnection : muduo::noncopyable 
{
public:
    using TcpConnectionPtr = std::shared_ptr<muduo::net::TcpConnection>;
    using BufferPtr = muduo::net::Buffer*;
    
    /**
     * 构造函数：创建SSL连接实例
     * @param conn TCP连接指针，用于底层网络通信
     * @param ctx SSL上下文指针，提供SSL配置和证书信息
     * 初始化SSL对象、BIO缓冲区和连接状态，设置为服务器接受模式
     */
    SslConnection(const TcpConnectionPtr& conn, SslContext* ctx);
    
    /**
     * 析构函数：清理SSL连接资源
     * 释放SSL对象和相关BIO缓冲区，确保资源正确回收
     */
    ~SslConnection();

    /**
     * 开始SSL握手过程
     * 设置SSL为接受状态并启动握手协议，等待客户端发起握手请求
     */
    void startHandshake();
    
    /**
     * 发送加密数据到对端
     * @param data 要发送的明文数据指针
     * @param len 数据长度（字节）
     * 将应用层数据通过SSL加密后发送到网络，处理SSL_write和BIO读取操作
     */
    void send(const void* data, size_t len);
    
    /**
     * 处理接收到的网络数据
     * @param conn TCP连接指针
     * @param buf 接收到的加密数据缓冲区
     * @param time 数据接收时间戳
     * 根据连接状态处理数据：握手阶段处理握手协议，已建立连接阶段解密数据
     */
    void onRead(const TcpConnectionPtr& conn, BufferPtr buf, muduo::Timestamp time);
    
    /**
     * 检查SSL握手是否完成
     * @return true表示握手已完成，可以开始加密通信；false表示握手仍在进行中
     */
    bool isHandshakeCompleted() const { return state_ == SSLState::ESTABLISHED; }
    
    /**
     * 获取解密后的数据缓冲区
     * @return 指向解密缓冲区的指针，包含已解密的应用程序数据
     */
    muduo::net::Buffer* getDecryptedBuffer() { return &decryptedBuffer_; }
    
    /**
     * BIO写操作回调函数
     * @param bio BIO结构指针
     * @param data 要写入的数据指针
     * @param len 数据长度
     * @return 实际写入的字节数，-1表示错误
     * 将SSL加密后的数据发送到网络连接
     */
    static int bioWrite(BIO* bio, const char* data, int len);
    
    /**
     * BIO读操作回调函数
     * @param bio BIO结构指针
     * @param data 读取数据缓冲区
     * @param len 缓冲区长度
     * @return 实际读取的字节数，-1表示无数据可读
     * 从网络连接读取数据供SSL解密使用
     */
    static int bioRead(BIO* bio, char* data, int len);
    
    /**
     * BIO控制操作回调函数
     * @param bio BIO结构指针
     * @param cmd 控制命令
     * @param num 数值参数
     * @param ptr 指针参数
     * @return 命令执行结果
     * 处理BIO的各种控制操作，如缓冲区刷新等
     */
    static long bioCtrl(BIO* bio, int cmd, long num, void* ptr);
    
    /**
     * 设置消息回调函数
     * @param cb 消息回调函数对象
     * 注册回调函数用于处理解密后的应用层数据
     */
    void setMessageCallback(const MessageCallback& cb) { messageCallback_ = cb; }
    
private:
    /**
     * 处理SSL握手过程
     * 执行SSL_do_handshake，处理握手状态转换和错误情况
     * 握手成功后记录加密套件和协议版本信息
     */
    void handleHandshake();
    
    /**
     * 处理加密数据发送
     * @param data 加密数据指针
     * @param len 数据长度
     * 将SSL加密后的数据添加到写缓冲区并发送到网络
     */
    void onEncrypted(const char* data, size_t len);
    
    /**
     * 处理解密数据接收
     * @param data 解密数据指针
     * @param len 数据长度
     * 将SSL解密后的应用数据存储到解密缓冲区
     */
    void onDecrypted(const char* data, size_t len);
    
    /**
     * 获取SSL操作的最后错误代码
     * @param ret SSL函数返回值
     * @return 对应的SSLError枚举值
     * 将OpenSSL错误代码转换为内部错误枚举
     */
    SSLError getLastError(int ret);
    
    /**
     * 处理SSL错误
     * @param error SSL错误类型
     * 根据错误类型采取相应措施，严重错误时关闭连接
     */
    void handleError(SSLError error);

private:
    SSL*                ssl_; ///< OpenSSL SSL对象，负责加密解密操作
    SslContext*         ctx_; ///< SSL上下文，提供配置和证书管理
    TcpConnectionPtr    conn_; ///< 底层TCP连接，用于网络数据传输
    SSLState            state_; ///< SSL连接状态（握手中/已建立/错误）
    BIO*                readBio_;   ///< 读BIO：网络数据 -> SSL解密
    BIO*                writeBio_;  ///< 写BIO：SSL加密 -> 网络数据
    muduo::net::Buffer  readBuffer_; ///< 读缓冲区：存储从网络接收的原始数据
    muduo::net::Buffer  writeBuffer_; ///< 写缓冲区：存储要发送的加密数据
    muduo::net::Buffer  decryptedBuffer_; ///< 解密缓冲区：存储解密后的应用数据
    MessageCallback     messageCallback_; ///< 消息回调：处理解密后的应用层数据
};

} // namespace ssl