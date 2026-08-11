#include <iostream>
#include <fstream>
#include <unistd.h>
#include <thread>
#include <chrono>
#include <utils.h> // 日志头文件

// 定义管道名称
#define PIPE_NAME "/tmp/x_pilot_pipe"

int main() {
    std::cout << "通信进程开始" << std::endl;

    // 尝试打开管道文件
    std::ifstream pipe(PIPE_NAME);

    // 如果打不开
    if (!pipe.is_open()) {
        std::cerr << "[Comm] Error: 无法打开管道" << PIPE_NAME << std::endl;
        return 1;
    }

    // 如果打开了
    std::cout << "管道连接成功, 等待数据..." << std::endl;

    std::string line;

    // 打开之后模拟管道读取数据
    while(true) {
        if (std::getline(pipe, line)) {
            std::cout << "收到控制中心的数据: " << line << "已发送到基站" << std::endl;
        } else {
            std::cout << "管道暂时没有数据, 等待中..." << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    return 0;
}
