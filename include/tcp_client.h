#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

#include <string>
#include <json.hpp>
#include <cstdint>
#include <vector>       // 处理粘包和半包的问题

using json = nlohmann::json;

// 状态机：断开 -> 连接中 -> 已连接
enum class TcpState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED
};

class TcpClient {
public:
    TcpClient();
    ~TcpClient();

    // 设置目标服务器
    void setTarget(const std::string& ip, int port);

    // 尝试连接（非阻塞），内部处理退避逻辑
    void tryConnect();

    // 发送数据（JSON + 帧头）
    bool send(const json& j);

    // 接收数据
    bool recv(json& out_json);

    // 关闭连接
    void close();

    // 获取当前状态
    TcpState getState() const { return m_state; }

    // 当 epoll 检测到可写时，确认连接是否真正成功
    void checkConnectionResult();

    // 当 epoll 检测到断开或错误时调用
    void onDisconnected();

    void handleFail(); 

    // 心跳相关
    void sendHeartbeat();         // 发送一次心跳包
    void checkHeartbeatTimeout(); // 检查是否超时（在主循环里调用）
    void onDataReceived();        // 收到数据时调用，重置计时器
    void resetHeartbeatTimer(); 

    int m_sock_fd = -1;

private:
    std::string m_ip;
    int m_port;
    TcpState m_state;

    // 重连退避控制
    int m_backoff_seconds;      // 当前等待秒数
    time_t m_last_attempt_time; // 上次尝试连接的时间点

    // 心跳控制
    int m_heartbeat_interval;   // 心跳间隔（秒），默认1
    int m_heartbeat_timeout;    // 心跳超时（秒），默认5
    time_t m_last_send_time;    // 上次发送心跳的时间
    time_t m_last_recv_time;    // 上次收到任何数据的时间

     // TCP 接收缓冲区（处理半包/粘包）
    std::vector<char> m_recv_buffer;  // 接收缓冲区，存放尚未解析的原始字节，全部的内容存放

    bool setNonBlocking(int fd);
};

#endif
