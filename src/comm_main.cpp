#include <iostream>
#include <unistd.h>

int main() {
    std::cout << "通信进程开始" << std::endl;

    // 模拟网络通信循环
    while(true) {
        std::cout << "[Comm] 检查 TCP 连接中..." << std::endl;
        sleep(1);
    }

    return 0;
}
