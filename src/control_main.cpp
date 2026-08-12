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
#include <ipc.h>        // IPC模块，通信

using json = nlohmann::json;

// 定义全局退出的标记
volatile sig_atomic_t g_should_exit = 0;

// 信号处理函数：收到信号后，将 g_should_exit 设为 0，让循环停止
void control_signal_handler(int signum) {
    g_should_exit = 1;
}

// 存放无人机配置文件
struct FlightConfig {
    std::string takeoffTime;
    double altitude;
    double latitude;
    double longitude;
    std::string shm_path;
    double speed;
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
        config.speed       = j.value("巡航速度", 0.0); 

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

    // 定义IPC服务端对象和客户端句柄
    UnixSocketServer server;
    int client_fd = -1;

    int shm_fd = -1;
    x_pilot::RobotState* shm_ptr = nullptr;

    // 获取当前时间戳的辅助函数
    uint64_t getTimestamp() {
        using namespace std::chrono;
        return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    }

    // 无限循环的飞控程序，防止退出
    void runMission(double target, double speed, const std::string& shm_path) {
        // 打印目标
        LOG_INFO("任务开始，目标高度：" + std::to_string(target) + "米");

        // 共享内存初始化c
        // 使用配置文件中的路径
        shm_fd = shm_open(shm_path.c_str(), O_CREAT | O_RDWR, 0666);
        if (shm_fd == -1) {
            LOG_ERROR("共享内存创建失败");
            return;
        }
        ftruncate(shm_fd, sizeof(x_pilot::RobotState));
        shm_ptr = (x_pilot::RobotState*)mmap(NULL, sizeof(x_pilot::RobotState), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
        if (shm_ptr == MAP_FAILED) {
            LOG_ERROR("共享内存映射失败");
            close(shm_fd);
            return;
        }
        LOG_INFO("共享内存挂载成功: " + shm_path);

        // 清零
        memset(shm_ptr, 0, sizeof(x_pilot::RobotState));

        // 初始化共享内存数据
        shm_ptr->speed = speed;

        // IPC服务启动
        if (!server.bindAndListen("/tmp/x_pilot.ipc")) {
            LOG_ERROR("IPC 服务端启动失败");
            return; // 启动失败直接退出
        }
        LOG_INFO("IPC 服务端已启动，等待连接...");

        // 循环等待客户端连接
        while (!g_should_exit) {
            fd_set read_fds;
            FD_ZERO(&read_fds);
            FD_SET(server.m_server_fd, &read_fds);
            struct timeval timeout {1, 0}; // 1秒超时

            int ret = select(server.m_server_fd + 1, &read_fds, NULL, NULL, &timeout);
            if (ret > 0) {
                if (FD_ISSET(server.m_server_fd, &read_fds)) {
                    client_fd = server.accept();
                    if (client_fd > 0) {
                        LOG_INFO("IPC 客户端已连接");
                        break; // 连接成功才跳出
                    }
                }
            } else if (ret < 0) {
                LOG_ERROR("Select 监听异常");
                return;
            }
        }

        // 多线程设置
        auto simulation_loop = [&]() {
            LOG_INFO("飞控仿真线程启动");
            int loop_counter = 0;

            while (!g_should_exit) {
                if (currentState == FlightState::CLIMBING) {
                    performClimb(target);
                } else if (currentState == FlightState::CRUISING) {
                    performCruise();
                }

                // 更新共享内存时间戳
                // 更新共享内存时间戳
                if (shm_ptr != nullptr) {
                    shm_ptr->timestamp = getTimestamp();

                    if (currentState == FlightState::CRUISING) {
                        // 注意：这里读取 speed，主线程可能会改 speed，但 double 读写通常是原子的，不影响大致逻辑
                        shm_ptr->x += shm_ptr->speed * 0.1;

                        if (shm_ptr->is_spraying) {
                            shm_ptr->sprayed_amount += shm_ptr->speed * 0.1;
                        }
                    }

                    loop_counter++;
                    if (loop_counter >= 10) {
                        loop_counter = 0;
                        if (shm_ptr->battary > 0) shm_ptr->battary -= 0.1;
                    }

                    LOG_INFO("[数据监控] 速度: " + std::to_string(shm_ptr->speed) + " | 喷洒: " + (shm_ptr->is_spraying ? "开" : "关"));
                }

                // 仿真逻辑结束
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            }
        };

        // 启动线程
        std::thread sim_thread(simulation_loop);

        LOG_INFO("主线程进入 IPC 指令监听模式");
        while (!g_should_exit) {
            if (client_fd > 0) {
                fd_set read_fds;
                FD_ZERO(&read_fds);
                FD_SET(client_fd, &read_fds);
                struct timeval timeout {1, 0};
                
                if (select(client_fd + 1, &read_fds, NULL, NULL, &timeout) > 0) {
                    if (FD_ISSET(client_fd, &read_fds)) {
                        json recv_j;
                        if (server.recvFrame(client_fd, recv_j)) {
                            handleCommand(recv_j);
                        } else {
                            LOG_WARN("客户端断开");
                            close(client_fd);
                            client_fd = -1;
                        }
                    }
                }
            } else {
                // 如果没有客户端连接，稍微睡一下避免 CPU 空转
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }

        // --- 6. 安全退出 ---
        if (sim_thread.joinable()) {
            sim_thread.join();
        }
        LOG_INFO("系统已安全退出");
    }

    // 处理接受客户端json指令
    void handleCommand(const json& cmd_json) {
        if (!cmd_json.contains("cmd")) {
            LOG_WARN("收到无效指令, 缺少cmd字段");
            return;
        }

        std::string cmd = cmd_json["cmd"];

        if (cmd == "SET_SPEED") {
            // 解析 params 中的 speed
            if (cmd_json.contains("params") && cmd_json["params"].contains("speed")) {
                double new_speed = cmd_json["params"]["speed"];
                shm_ptr->speed = new_speed; // 修改共享内存
                LOG_INFO("指令执行：速度设置为 " + std::to_string(new_speed));
            }
        } else if (cmd == "START_SPRAY") {
            shm_ptr->is_spraying = true; // 开始喷洒
            LOG_INFO("指令执行：开始喷洒");
        } else {
            LOG_WARN("收到未知指令：" + cmd);
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

    // 发送到IPC的数据
    void sendTelemetry() {
        if (client_fd > 0) {
            json j;
            j["altitude"] = currentAltitude;
            j["state"] = (currentState == FlightState::CLIMBING ? "CLIMBING" : "CRUISING");
            
            // 调用我们封装好的发送函数
            if (!server.sendFrame(client_fd, j)) {
                LOG_ERROR("IPC 数据发送失败");
            }
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
    std::ifstream sysConf("config/config.json");
    json sys_j;
    sysConf >> sys_j;
    std::string shm_path = sys_j.value("shm_path", "/x_pilot_shm");

    // 读取飞机配置 (拿任务参数)
    FlightConfig myConfig = loadConfig(); // 这里的 loadConfig 保持读取 config_plane.json 不变

    // 启动 (传入从 sysConf 拿到的路径)
    FlightController drone;
    drone.runMission(myConfig.altitude, myConfig.speed, shm_path);
    
    return 0;
}