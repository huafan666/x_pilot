#include <iostream>
#include <fstream>
#include <unistd.h>
#include <thread>
#include <chrono>
#include <utils.h>      // 日志头文件
#include <csignal>      // 信号
#include <poll.h>       // 设置等待时间
#include <fcntl.h>
#include <sys/types.h>

// 定义管道名称
#define PIPE_NAME "/tmp/x_pilot_pipe"

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

    // 尝试打开管道文件
    int pipe_fd = open(PIPE_NAME, O_RDONLY | O_NONBLOCK);

    // 如果打不开
    if (pipe_fd == -1) {
        std::cerr << "[Comm] Error: 无法打开管道" << PIPE_NAME << std::endl;
        return 1;
    }

    // 如果打开了
    std::cout << "管道连接成功, 等待数据..." << std::endl;

    // 定义结构体
    struct pollfd fds[1];
    fds[0].fd = pipe_fd;
    fds[0].events = POLLIN;

    // 读取缓冲
    char buffer[1024];

    // 打开之后模拟管道读取数据
    while(!g_should_exit) {
        // 设置超时时间
        int ret = poll(fds, 1, -1);
        
        if (ret > 0) {
            if (fds[0].revents & POLLIN) {
                ssize_t bytes_read = read(pipe_fd, buffer, sizeof(buffer) - 1);
                if (bytes_read > 0) {
                    buffer[bytes_read] = '\0';
                    std::cout << "收到控制中心的数据: " << buffer << "已发送到基站" << std::endl;
                } else if (bytes_read == 0) {
                    // 读到0字节，通常意味着写端关闭了
                    std::cout << "[Comm] 管道写端已关闭" << std::endl;
                    break; 
                }
            }
        } else if (ret == -1) {
            // 如果错误是 EINTR，说明是被信号打断的
            if (errno == EINTR) {
                continue; 
            }
            perror("poll 出错");
            LOG_ERROR("poll 出错");
            break;
        }
    }

    return 0;
}
