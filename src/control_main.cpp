#include <iostream>
#include <sstream>
#include <string>
#include <ctime>
#include <chrono>
#include <iomanip>
#include <fstream>      // 文件流
#include <json.hpp>     // 引入json库
#include <thread>       // 用于让程序睡觉
#include <utils.h>      // 日志函数
#include <types.h>      // 数据类型
#include <sys/mman.h>   // linux内存管理接口
#include <fcntl.h>      // 控制文件
#include <unistd.h>     
#include <csignal>      // 信号

using json = nlohmann::json;

// 定义全局退出的标记
volatile sig_atomic_t g_should_exit = 0;

// 信号处理函数：收到信号后，将 g_should_exit 设为 0，让循环停止
void control_signal_handler(int signum) {
    g_should_exit = 1;
}

// 管道文件
#define PIPE_NAME "/tmp/x_pilot_pipe" // <--- 必须有：和 Manager 一致的名字

// 存放无人机配置文件
struct FlightConfig {
    std::string takeoffTime;
    double altitude;
    double latitude;
    double longitude;
};

// 读取无人机配置文件的JSON对象，然后返回FlightConfig对象
FlightConfig loadConfig() {
    FlightConfig config;

    std::ifstream configFile("config/config_plane.json");
    if (!configFile.is_open()) {
        LOG_ERROR("无法打开 config/config_plane.json");
        return config;
    }   

    try {
        json j;
        configFile >> j;
        config.takeoffTime = j.value("起飞时间", "未知时间");
        config.altitude = j.value("巡航高度", 0.0);
        config.latitude = j.value("经度", 0.0);
        config.longitude = j.value("纬度", 0.0);

        LOG_INFO("配置文件加载成功");
    } catch (const std::exception& e) {
        LOG_ERROR("JSON 解析失败：" + std::string(e.what()));
    }

    return config;
}

// 飞机的状态定义
enum class FlightState {
    CLIMBING,  // 爬升中
    CRUISING,  // 巡航中
    DESCENDING // 降落中
};

// 飞行控制类
// 模拟飞机的物理状态
class FlightController {
public:
    // 导入头文件中的结构体
    x_pilot::RobotState state;

    double currentAltitude = 0.0; // 当前高度
    FlightState currentState = FlightState::CLIMBING;

    // 定义一个管道输出流
    std::ofstream dataPipe;

    int shm_fd = -1;
    x_pilot::RobotState* shm_ptr = nullptr;

    // 无限循环的飞控程序，防止退出
    void runMission(double target) {
        LOG_INFO("任务开始，目标高度：" + std::to_string(target) + "米");

        const char* shm_name = "/x_pilot_shm";

        // 打开共享内存, 准备读取内容
        shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
        if (shm_fd == -1) {
            LOG_ERROR("共享内存创建失败");
            return;
        }

        // 创建成功的话, 设置大小
        ftruncate(shm_fd, sizeof(x_pilot::RobotState));

        // 映射到内存中
        shm_ptr = (x_pilot::RobotState*)mmap(NULL, sizeof(x_pilot::RobotState), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
        if (shm_ptr == MAP_FAILED) {
            LOG_ERROR("共享内存映射失败");
            close(shm_fd);
            return;
        }
        LOG_INFO("共享内存挂载成功");

        // 打开管道
        dataPipe.open(PIPE_NAME);
        if (!dataPipe.is_open()) {
            LOG_ERROR("无法连接到管道");
        } else {
            LOG_INFO("数据链路已建立");
        }

        // 循环执行高度判断和飞行逻辑
        while (!g_should_exit) {
            if (currentState == FlightState::CLIMBING) {
                performClimb(target);
            }
            else if (currentState == FlightState::CRUISING) {
                performCruise();
            }

            // 休息时间，防止频率过高
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

private:
    // 动作1：爬升
    void performClimb(double target) {
        if (currentAltitude < target) {
            currentAltitude += 10.0;

            // 高度currentAltitude赋值给结构体的z
            if (shm_ptr != nullptr) {
                shm_ptr->z = currentAltitude;
                LOG_INFO("赋值成功, 高度给z值");
            } else {
                LOG_ERROR("赋值失败, 未知原因");
            }

            LOG_INFO("正在爬升…… 当前高度为：" + std::to_string(currentAltitude) + "米");
            sendTelemetry();
        }
        else {
            currentState = FlightState::CRUISING;
            LOG_INFO("到达目标高度，切换巡航模式");
        }
    }

    // 动作2：巡航
    void performCruise() {
        LOG_INFO("巡航中…… 系统正常，高度为：" + std::to_string(currentAltitude) + "米");
    }

    // 发送到管道的数据
    void sendTelemetry() {
        if (dataPipe.is_open()) {
            std::string msg = "ALT:" + std::to_string(currentAltitude);
            // 写入管道，并加换行
            dataPipe << msg << std::endl;
        }
    }
};

// main
int main() {
    LOG_INFO("系统初始化……");

    // 注册信号处理函数，捕获 Manager 发来的 SIGTERM 和 Ctrl+C 的 SIGINT
    signal(SIGTERM, control_signal_handler);
    signal(SIGINT, control_signal_handler);
    
    // 加载配置文件
    FlightConfig myConfig = loadConfig();

    // 创建飞行控制器对象，并启动飞控
    FlightController drone;

    // 执行飞行任务
    drone.runMission(myConfig.altitude);
    
    return 0;
}