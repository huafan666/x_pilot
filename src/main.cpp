#include <iostream>

int main() {
    std::cout << "System Startup..." << std::endl;
    std::cout << "Hello X-Pilot!" << std::endl;

    // 定义变量
    int throttle = 0; // 油门 0 - 100
    int yaw = 0;      // 偏航 -90 ~ 90
    
    // 读取用户输入
    std::cout << "请输入油门 (0 - 100): ";
    std::cin >> throttle;

    std::cout << "请输入方向 (-90 - 90): ";
    std::cin >> yaw;

    // 简单的逻辑判断
    std::cout << "--- System Status ---" << std::endl;
    std::cout << "Throttle: " << throttle << "%" << std::endl;
    std::cout << "Yaw" << yaw << " deg" << std::endl;

    if (throttle > 80) {
        std::cout << "警告：油门过高！" << std::endl;
    } else if (throttle > 0) {
        std::cout << "状态：飞行中……" << std::endl;
    } else {
        std::cout << "状态：待机" << std::endl;
    }

    return 0;
}