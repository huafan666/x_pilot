#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <types.h>
#include <utils.h>
#include <raii.h>

int main() {
    std::cout << "Plane View 启动中..." << std::endl;
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

    // 读取数据（原样不变）
    while (true) {
        double alt = shm_ptr->z;
        std::cout << "当前高度为: " << alt << "米" << std::endl;
        sleep(1);
    }

    // 不用手动 munmap + close，shm 析构自动清理
    return 0;
}