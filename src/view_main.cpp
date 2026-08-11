#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <types.h>
#include <utils.h>

int main() {
    std::cout << "Plane View 启动中..." << std::endl;

    const char* shm_name = "/x_pilot_shm";
    int shm_fd = -1;
    x_pilot::RobotState* shm_ptr = nullptr;

    // 打开共享内存
    shm_fd = shm_open(shm_name, O_RDONLY, 0666);
    
    // 如果失败
    if (shm_fd == -1) {
        std::cerr << "无法打开内存" << std::endl;
        LOG_ERROR("无法打开内存, 无法可视化");
        return 1;
    }

    // 如果成功
    std::cout << "内存打开成功" << std::endl;

    // 内存映射
    shm_ptr = (x_pilot::RobotState*)mmap(NULL, sizeof(x_pilot::RobotState), PROT_READ, MAP_SHARED, shm_fd, 0);

    // 检查是否映射成功
    if (shm_ptr == MAP_FAILED) {
        std::cerr << "内存映射失败" << std::endl;
        LOG_ERROR("内存映射失败");
        close(shm_fd);
        return 1;
    }

    // 映射成功
    std::cout << "内存映射成功" << std::endl;
    LOG_INFO("内存映射成功");

    // 读取数据
    while (true) {
        double alt = shm_ptr->z;
        std::cout << "当前高度为: " << alt << "米" << std::endl;
        sleep(1);
    }

    munmap(shm_ptr, sizeof(x_pilot::RobotState));
    close(shm_fd);

    return 0;
}