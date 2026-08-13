#include "tcp_client.h"
#include "utils.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include <time.h>
#include "protocol.h"
#include "metrics.h"

#define FRAME_HEADER_LEN 4

// 初始化tcp相关变量
TcpClient::TcpClient() 
    : m_sock_fd(-1), m_port(0), m_state(TcpState::DISCONNECTED)
    , m_backoff_seconds(1), m_last_attempt_time(0)
    , m_heartbeat_interval(1), m_heartbeat_timeout(5)
    , m_last_send_time(0), m_last_recv_time(0) {}

TcpClient::~TcpClient() {
    close();
}

void TcpClient::setTarget(const std::string& ip, int port) {
    m_ip = ip;
    m_port = port;
}

bool TcpClient::setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return false;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1;
}

void TcpClient::tryConnect() {
    // 1. 如果已经连接成功，无需重试
    if (m_state == TcpState::CONNECTED) return;

    // 2. 检查退避时间（指数退避核心）
    time_t now = time(nullptr);
    if (now - m_last_attempt_time < m_backoff_seconds) {
        return; // 时间没到，继续等待
    }

    // 3. 清理旧 socket
    if (m_sock_fd != -1) {
        ::close(m_sock_fd);
        m_sock_fd = -1;
    }

    // 4. 创建新 Socket
    m_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_sock_fd == -1) {
        LOG_ERROR("TCP: socket() 失败");
        return;
    }

    if (!setNonBlocking(m_sock_fd)) {
        LOG_ERROR("TCP: 设置失败");
        ::close(m_sock_fd);
        m_sock_fd = -1;
        return;
    }

    // 5. 发起连接
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(m_port);
    
    if (inet_pton(AF_INET, m_ip.c_str(), &addr.sin_addr) <= 0) {
        LOG_ERROR("TCP: 不可用的 IP: " + m_ip);
        ::close(m_sock_fd);
        m_sock_fd = -1;
        return;
    }

    int ret = ::connect(m_sock_fd, (struct sockaddr*)&addr, sizeof(addr));

    if (ret == 0) {
        // 极少数情况立即成功
        m_state = TcpState::CONNECTED;
        m_backoff_seconds = 1; // 重置退避
        LOG_INFO("tcp连接成功至 " + m_ip + ":" + std::to_string(m_port));

        // 埋点：TCP 连接成功
        Metrics::emit("tcp_connect_success", {
            {"server_ip", m_ip},
            {"server_port", std::to_string(m_port)}
        });
    } else if (errno == EINPROGRESS) {
        // 正在连接中（非阻塞模式正常返回）
        m_state = TcpState::CONNECTING;
        m_last_attempt_time = now; 
        LOG_INFO("TCP: 连接中...");
    } else {
        // 立即失败（如拒绝连接）
        LOG_ERROR(std::string("TCP: 连接失败 ") + strerror(errno));
        handleFail();
    }
}

// 辅助函数：处理失败逻辑（更新退避时间）
void TcpClient::handleFail() {
    close();
    m_state = TcpState::DISCONNECTED;
    m_last_attempt_time = time(nullptr);

    // 指数退避：1->2->4->...->60
    m_backoff_seconds *= 2;
    if (m_backoff_seconds > 60) m_backoff_seconds = 60;

    LOG_ERROR("tcp连接失败, 重新尝试 " + std::to_string(m_backoff_seconds) + "s");
    // 埋点：TCP 连接失败
    Metrics::emit("tcp_connect_fail", {
        {"server_ip", m_ip},
        {"server_port", std::to_string(m_port)},
        {"backoff", std::to_string(m_backoff_seconds)}
    });
}

// 当 epoll 监听到 socket 可写时，调用此函数确认是否真的连上了
void TcpClient::checkConnectionResult() {
    if (m_state != TcpState::CONNECTING) return;

    int error = 0;
    socklen_t len = sizeof(error);
    if (getsockopt(m_sock_fd, SOL_SOCKET, SO_ERROR, &error, &len) == -1) {
        handleFail();
        return;
    }

    if (error == 0) {
        // 连接成功
        m_state = TcpState::CONNECTED;
        m_backoff_seconds = 1; // 成功后重置等待时间
        LOG_INFO("tcp连接成功 " + m_ip + ":" + std::to_string(m_port));
    } else {
        // 连接失败
        LOG_ERROR(std::string("TCP: 异步连接失败: ") + strerror(error));
        handleFail();
    }
}

// 发送心跳包
void TcpClient::sendHeartbeat() {
    // 只有已连接状态才发
    if (m_state != TcpState::CONNECTED || m_sock_fd == -1) return;

    time_t now = time(nullptr);

    // 距离上次发心跳不到 1 秒，不发
    if (now - m_last_send_time < m_heartbeat_interval) return;

    // 构造心跳 JSON（PRD 5.2.1 格式）
    json heartbeat;
    heartbeat["type"] = "heartbeat";
    heartbeat["timestamp"] = (long long)now;

    // 复用已有的 send 方法发送
    if (!send(heartbeat)) {
        LOG_WARN("心跳发送失败");
        // send 内部失败会调用 onDisconnected，这里不用再处理
        return;
    }

    m_last_send_time = now;
    LOG_DEBUG("心跳已发送");
}

// 检查心跳是否超时（主循环每次循环调用）
void TcpClient::checkHeartbeatTimeout() {
    // 只有已连接状态才检查
    if (m_state != TcpState::CONNECTED) return;

    time_t now = time(nullptr);

    // 如果从来没收到过数据（m_last_recv_time == 0）
    // 就从连接成功那一刻开始算超时
    if (m_last_recv_time == 0) {
        m_last_recv_time = m_last_send_time; // 退而求其次，用发送时间
    }

    // 距离上次收到数据超过 5 秒 → 判定断线
    if (now - m_last_recv_time >= m_heartbeat_timeout) {
        LOG_WARN("心跳超时，服务器 " + m_ip + ":" + std::to_string(m_port) + " 无响应，判定断线");
        // 埋点：心跳超时
        Metrics::emit("heartbeat_timeout", {
            {"server_ip", m_ip},
            {"server_port", std::to_string(m_port)}
        });
        onDisconnected();  // 复用已有的断线处理（会触发重连）
    }
}

// 收到服务器任何数据时调用，重置超时计时器
void TcpClient::onDataReceived() {
    m_last_recv_time = time(nullptr);
    LOG_DEBUG("收到数据，重置心跳计时器");
}

void TcpClient::onDisconnected() {
    if (m_state != TcpState::DISCONNECTED) {
        LOG_WARN("TCP: 连接断开");
        handleFail();
    }
}

void TcpClient::close() {
    if (m_sock_fd != -1) {
        ::close(m_sock_fd);
        m_sock_fd = -1;
    }
    m_state = TcpState::DISCONNECTED;
}

void TcpClient::resetHeartbeatTimer() {
    m_last_send_time = time(nullptr);
    m_last_recv_time = time(nullptr);
}

bool TcpClient::send(const json& j) {
    if (m_state != TcpState::CONNECTED || m_sock_fd == -1) return false;

    std::string body = j.dump();
    uint32_t len = htonl(body.length());

    // 发送头
    if (::send(m_sock_fd, &len, FRAME_HEADER_LEN, 0) != FRAME_HEADER_LEN) {
        onDisconnected();
        return false;
    }
    // 发送体
    if (::send(m_sock_fd, body.c_str(), body.length(), 0) != (ssize_t)body.length()) {
        onDisconnected();
        return false;
    }
    return true;
}
// 从 socket 读原始数据到缓冲区，再尝试拆出一个完整包
bool TcpClient::recv(json& out_json) {
    if (m_sock_fd == -1) return false;

    // 第一步：从 socket 读数据，追加到缓冲区
    char tmp[4096];
    while (true) {
        ssize_t n = ::recv(m_sock_fd, tmp, sizeof(tmp), 0);
        if (n > 0) {
            m_recv_buffer.insert(m_recv_buffer.end(), tmp, tmp + n);
            // 非阻塞模式，可能还有数据，继续读直到 EAGAIN
        } else if (n == 0) {
            // 对方关闭连接
            onDisconnected();
            return false;
        } else {
            // n < 0
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;  // 没有更多数据了，正常
            } else if (errno == EINTR) {
                continue;  // 被信号打断，重试
            } else {
                onDisconnected();
                return false;  // 真正的错误
            }
        }
    }

    // 如果缓冲区是空的，说明这次 epoll 可读事件没读到东西
    if (m_recv_buffer.empty()) {
        return false;
    }

    // 第二步：尝试从缓冲区拆出一个完整包
    return parseFrame(m_recv_buffer, out_json);
}