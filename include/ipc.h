#ifndef X_PILOT_IPC_H
#define X_PILOT_IPC_H

#include <string>
#include <cstdint>
#include <sys/un.h> // 用于 sockaddr_un (Unix socket 结构体)
#include <json.hpp>

using json = nlohmann::json;

// 定义帧头长度为4字节
const int FRAME_HEADER_LEN = 4;

// UDS服务端
class UnixSocketServer {
public:
    UnixSocketServer();
    ~UnixSocketServer();

    // 绑定和监听
    bool bindAndListen(const std::string& path);

    // 接受连接
    int accept();

    // 发送数据
    bool sendFrame(int client_fd, const json& j);

    // 接受数据
    bool recvFrame(int client_fd, json& out_json);
    
    // 关闭服务
    void closeServer();

    // 服务编号
    int m_server_fd = -1;

private:
    std::string m_path;
};

class UnixSocketClient {
public:
    UnixSocketClient();
    ~UnixSocketClient();

    bool connectToServer(const std::string& path);
    bool recvFrame(json& j);
    bool sendFrame(const json& j);
    void close();

    int m_sock_fd = -1; // 统一命名
};

#endif