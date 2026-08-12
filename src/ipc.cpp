#include <ipc.h>
#include <utils.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <arpa/inet.h>

// ==================== UnixSocketServer 实现 ====================

UnixSocketServer::UnixSocketServer() : m_server_fd(-1) {

}
UnixSocketServer::~UnixSocketServer() {
    closeServer();
}

bool UnixSocketServer::bindAndListen(const std::string& path) {
    m_path = path;

    // 创建 socket
    m_server_fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (m_server_fd == -1) {
        LOG_ERROR("Server: socket 创建失败");
        return false;
    }

    // 绑定地址
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);

    unlink(path.c_str());

    // 开始绑定
    if (bind(m_server_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        LOG_ERROR("Server: bind 失败");
        close(m_server_fd);
        m_server_fd = -1;
        return false;
    }

    // 开始监听
    if (listen(m_server_fd, 5) == -1) {
        LOG_ERROR("Server: Listen 失败");
        close(m_server_fd);
        m_server_fd = -1;
        return false;
    }

    LOG_INFO("Server: Listen 成功" + path);
    return true;
}

int UnixSocketServer::accept() {
    if (m_server_fd ==  -1) {
        return -1;
    }

    struct sockaddr_un client_addr;
    socklen_t client_len = sizeof(client_addr);

    int client_fd = ::accept(m_server_fd, (struct sockaddr*)&client_addr, &client_len);

    if (client_fd == -1) {
        LOG_ERROR("Server: accept 失败");
        return -1;
    }

    LOG_INFO("Server: accept 成功");
    return client_fd;
}

bool UnixSocketServer::sendFrame(int client_fd, const json& j) {
    // 将输入的json内容转化为字符串
    std::string body = j.dump();

    // 计算长度
    uint32_t len = htonl(body.length());

    // 发送4字节长度头
    ssize_t sent = ::send(client_fd, &len, FRAME_HEADER_LEN, 0);
    if (sent != FRAME_HEADER_LEN) {
        LOG_ERROR("发送帧头失败");
        return false;
    }

    // 如果发送头成功，则发送json实体
    sent = ::send(client_fd, body.c_str(), body.length(), 0);
    if (sent != (ssize_t)body.length()) {
        LOG_ERROR("发送帧体失败");
        return false;
    }

    return true;
}

bool UnixSocketServer::recvFrame(int client_fd, json& out_json) {
    // 先读4字节头
    uint32_t len_net;
    ssize_t n = ::recv(client_fd, &len_net, FRAME_HEADER_LEN, MSG_WAITALL);
    uint32_t len = ntohl(len_net);
    // 读取失败
    if (n == 0) return false;
    if (n != FRAME_HEADER_LEN) return false;
    if (len > 1024 * 1024) {
        LOG_ERROR("收到超大帧长度，拒绝接收: " + std::to_string(len));
        return false;
    }

    // 存放读取成功的内容
    std::vector<char> buffer(len + 1);

    n = ::recv(client_fd, buffer.data(), len, MSG_WAITALL);

    // 再次读取失败
    if (n <= 0 || n != (ssize_t)len) {
        return false; // 读失败或没读够
    }

    buffer[len] = '\0';

    // 读取成功，解析json数据
    try {
        out_json = json::parse(buffer.data());
        return true;
    } catch (const json::parse_error& e) {
        LOG_ERROR("JSON 解析失败: " + std::string(e.what()));
        return false;
    }
}

void UnixSocketServer::closeServer() {
    if (m_server_fd != -1) {
        ::close(m_server_fd);
        m_server_fd = -1;
    }
}
// ==================== UnixSocketClient 实现 ====================

// 构造函数：初始化，把 fd 设为无效
UnixSocketClient::UnixSocketClient() : m_sock_fd(-1) {
}

// 析构函数：退出时自动断开
UnixSocketClient::~UnixSocketClient() {
    close();
}

// 连接服务端
bool UnixSocketClient::connectToServer(const std::string& path) {
    m_sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (m_sock_fd == -1) {
        LOG_ERROR("Client: socket 创建失败");
        return false;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);

    // 连接
    if (connect(m_sock_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        LOG_ERROR("Client: 连接失败，服务端可能未启动: " + path);
        ::close(m_sock_fd);
        m_sock_fd = -1;
        return false;
    }

    LOG_INFO("Client: 连接成功: " + path);
    return true;
}

// 发送数据
bool UnixSocketClient::sendFrame(const json& j) {
    std::string body = j.dump();
    uint32_t len = htonl(body.length());
    if (::send(m_sock_fd, &len, 4, 0) != 4) return false;
    if (::send(m_sock_fd, body.c_str(), body.length(), 0) != (ssize_t)body.length()) return false;
    return true;
}

// 接收数据
bool UnixSocketClient::recvFrame(json& out_json) {
    uint32_t len_net;
    if (::recv(m_sock_fd, &len_net, 4, MSG_WAITALL) != 4) return false;
    uint32_t len = ntohl(len_net);
    if (len > 1024 * 1024) return false;
    
    std::vector<char> buffer(len + 1);
    if (::recv(m_sock_fd, buffer.data(), len, MSG_WAITALL) != (ssize_t)len) return false;
    
    buffer[len] = '\0';
    try {
        out_json = json::parse(buffer.data());
        return true;
    } catch (...) {
        return false;
    }
}
// 关闭连接
void UnixSocketClient::close() {
    if (m_sock_fd != -1) {
        ::close(m_sock_fd);
        m_sock_fd = -1;
    }
}