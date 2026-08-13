#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <types.h>
#include <utils.h>
#include <raii.h>
#include <csignal>

// 定义全局退出的标记
volatile sig_atomic_t g_should_exit = 0;

// 信号处理函数：收到信号后，将 g_should_exit 设为 1，让循环停止
void view_signal_handler(int signum) {
    g_should_exit = 1;
}

int main() {
    std::cout << "Plane View 启动中..." << std::endl;

    // 注册信号处理函数
    signal(SIGTERM, view_signal_handler);
    signal(SIGINT, view_signal_handler);
    signal(SIGPIPE, SIG_IGN);

    sleep(2);

    // 用 RAII 封装共享内存（只读）
    ScopedShm shm;
    if (!shm.open("/x_pilot_shm", sizeof(x_pilot::RobotState), true)) {
        std::cerr << "无法打开/映射内存" << std::endl;
        LOG_ERROR("无法打开内存, 无法可视化");
        return 1;
    }

    std::cout << "内存映射成功" << std::endl;
    LOG_INFO("内存映射成功");

    x_pilot::RobotState* shm_ptr = (x_pilot::RobotState*)shm.get();

    // 读取数据
    while (!g_should_exit) {
        double alt = shm_ptr->z;
        std::cout << "当前高度为: " << alt << "米" << std::endl;
        sleep(1);
    }

    // 不用手动 munmap + close，shm 析构自动清理
    LOG_INFO("view 进程退出");
    return 0;
}
