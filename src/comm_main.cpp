#include <iostream>
#include <string>
#include <fstream>
#include <unistd.h>
#include <thread>
#include <chrono>
#include <utils.h>      // 日志头文件
#include <csignal>      // 信号
#include <sys/types.h>
#include "tcp_client.h" // TCP客户端
#include "ipc.h"        // IPC 客户端
#include "config.h"     // 配置加载器
#include <sys/epoll.h>  // Epoll网络通信
#include <json.hpp>

using json = nlohmann::json;

// 定义全局退出的标记
volatile sig_atomic_t g_should_exit = 0;

// 信号处理函数：收到信号后，将 g_should_exit 设为 0，让循环停止
void control_signal_handler(int signum) {
    g_should_exit = 1;
}

int main() {
    // 注册信号处理函数，捕获 Manager 发来的 SIGTERM 和 Ctrl+C 的 SIGINT
    signal(SIGTERM, control_signal_handler);
    signal(SIGINT, control_signal_handler);
    
    std::cout << "通信进程开始" << std::endl;

    // 初始化配置
    Config cfg = ConfigLoader::load("config/config.json");
    LOG_INFO("参数加载目标" + cfg.server_ip + ":" + std::to_string(cfg.server_port));

    // 初始化tcp客户端连接地面站
    TcpClient tcp_client;
    tcp_client.setTarget(cfg.server_ip, cfg.server_port);

    // 初始化ipc客户端连接control
    UnixSocketClient ipc_client;
    std::string ipc_path = "/tmp/x_pilot.ipc";
    if (!ipc_client.connectToServer(ipc_path)) {
        LOG_WARN("IPC连接失败");
    } else {
        LOG_INFO("IPC连接成功"); 
    }

    // 创建epoll
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        LOG_ERROR("epoll创建失败");
        return 1;
    }

    // Epoll 事件结构体
    struct epoll_event ev, events[10];
    
    std::cout << "进入主循环，等待数据..." << std::endl;

    // --- 5. 主循环 ---
    while(!g_should_exit) {
        // A. 处理 TCP 连接逻辑 (重连)
        if (tcp_client.getState() == TcpState::DISCONNECTED) {
            tcp_client.tryConnect();
            // 如果成功发起连接(状态变为 CONNECTING 或 CONNECTED)，加入 epoll 监听
            if (tcp_client.getState() != TcpState::DISCONNECTED) {
                ev.events = EPOLLIN | EPOLLOUT; // 监听可读和可写(可写用于检测连接结果)
                ev.data.fd = tcp_client.m_sock_fd;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, tcp_client.m_sock_fd, &ev) == -1) {
                    LOG_ERROR("epoll_ctl add tcp failed");
                    tcp_client.close();
                }
            }
        }

        // B. 处理 IPC 连接逻辑 (重连)
        if (ipc_client.m_sock_fd == -1) {
            if (ipc_client.connectToServer(ipc_path)) {
                std::cout << "IPC 重连成功" << std::endl;
                ev.events = EPOLLIN;
                ev.data.fd = ipc_client.m_sock_fd;
                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, ipc_client.m_sock_fd, &ev);
            }
        }

        // C. 等待事件 (超时 1秒，以便循环回来检查重连)
        int timeout_ms = 1000; 
        int nfds = epoll_wait(epoll_fd, events, 10, timeout_ms);

        if (nfds == -1) {
            if (errno == EINTR) continue; // 被信号中断
            LOG_ERROR("epoll_wait error");
            break;
        }

        // D. 处理发生的事件
        for (int i = 0; i < nfds; ++i) {
            int fd = events[i].data.fd;

            // --- Case 1: TCP 事件 ---
            if (fd == tcp_client.m_sock_fd) {
                // 1.1 处理可写事件 (检测连接结果)
                if (events[i].events & EPOLLOUT) {
                    if (tcp_client.getState() == TcpState::CONNECTING) {
                        tcp_client.checkConnectionResult();
                        
                        // 如果连接成功，修改 epoll 监听事件，移除 EPOLLOUT，只监听 EPOLLIN
                        if (tcp_client.getState() == TcpState::CONNECTED) {
                            ev.events = EPOLLIN;
                            ev.data.fd = fd;
                            epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev);
                        }
                    }
                }

                // 1.2 处理可读事件 (收到地面站数据)
                if (events[i].events & EPOLLIN) {
                    json j;
                    if (tcp_client.recv(j)) {
                        // 收到地面站指令 -> 转发给 Control
                        // 注意：这里如果 IPC 断开，发送会失败，我们在循环里会自动重连 IPC
                        if (ipc_client.m_sock_fd != -1) {
                            ipc_client.sendFrame(j); 
                            // std::cout << "TCP->IPC: " << j.dump() << std::endl;
                        }
                    } else {
                        // recv 返回 false，通常意味着连接断开
                        tcp_client.onDisconnected();
                        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
                    }
                }
                
                // 1.3 处理错误或挂起
                if (events[i].events & (EPOLLERR | EPOLLHUP)) {
                    tcp_client.onDisconnected();
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
                }
            }
            
            // --- Case 2: IPC 事件 ---
            else if (fd == ipc_client.m_sock_fd) {
                if (events[i].events & EPOLLIN) {
                    json j;
                    if (ipc_client.recvFrame(j)) {
                        // 收到 Control 状态 -> 转发给 地面站
                        if (tcp_client.getState() == TcpState::CONNECTED) {
                            tcp_client.send(j);
                            // std::cout << "IPC->TCP: " << j.dump() << std::endl;
                        }
                    } else {
                        // IPC 断开
                        LOG_WARN("IPC disconnected");
                        close(fd);
                        // m_sock_fd 会自动变成 -1，下次循环会重连
                    }
                }
                else if (events[i].events & (EPOLLERR | EPOLLHUP)) {
                    close(fd);
                }
            }
        }
    }

    // --- 6. 清理资源 ---
    close(epoll_fd);
    tcp_client.close();
    ipc_client.close();
    std::cout << "通信进程退出" << std::endl;

    return 0;
}
