#include <iostream>
#include <sstream>
#include <string>
#include <ctime>
#include <chrono>
#include <iomanip>
#include <fstream>   // 文件流
#include <json.hpp>  // 引入json库
#include <thread>    // 用于让程序睡觉

using json = nlohmann::json;

// 日志风险等级
enum class LogLevel {
    INFO,          // 普通
    WARNING,       // 警告
    ERROR          // 错误
};

// 日志风险等级划分的辅助函数
std::string levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::INFO:   return "INFO";
        case LogLevel::WARNING:   return "INFO";
        case LogLevel::ERROR:   return "INFO";
        default: return "UNKNOWN";
    }
}

// 记录日志函数
void logMessage(LogLevel level, const std::string& message) {
    // 获取当前时间
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:%M:%S");

    // 拼接日志内容
    std::string logEntry = "[" + ss.str() + "][" + levelToString(level) + "]" + message;
    
    // 打开日志文件追加内容
    std::ofstream logfile("log/x_pilot.log", std::ios::app);
    if (logfile.is_open()) {
        logfile << logEntry << std::endl;
        std::cout << "日志已写入：" << logEntry << std::endl;
        logfile.close();
    } else {
        std::cout << "错误，无法打开日志文件！" << std::endl;
    }

    // 特殊情况：日志包含错误内容
    if (level == LogLevel::ERROR) {
        std::cout << "发现严重错误！！！" << logEntry << std::endl; 
    }
}

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

    std::ifstream configFile("config/config.json");
    if (!configFile.is_open()) {
        logMessage(LogLevel::ERROR, "无法打开 config/config.json");
        return config;
    }   

    try {
        json j;
        configFile >> j;
        config.takeoffTime = j.value("起飞时间", "未知时间");
        config.altitude = j.value("巡航高度", 0.0);
        config.latitude = j.value("经度", 0.0);
        config.longitude = j.value("纬度", 0.0);

        logMessage(LogLevel::INFO, "配置文件加载成功");
    } catch (const std::exception& e) {
        logMessage(LogLevel::ERROR, "JSON 解析失败：" + std::string(e.what()));
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
    double currentAltitude = 0.0; // 当前高度
    FlightState currentState = FlightState::CLIMBING;

    // 无限循环的飞控程序，防止退出
    void runMission(double target) {
        logMessage(LogLevel::INFO, "任务开始，目标高度：" + std::to_string(target) + "米");

        while (true) {
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
            logMessage(LogLevel::INFO, "正在爬升…… 当前高度为：" + std::to_string(currentAltitude) + "米");
        }
        else {
            currentState = FlightState::CRUISING;
            logMessage(LogLevel::INFO, "到达目标高度，切换巡航模式");
        }
    }

    // 动作2：巡航
    void performCruise() {
        logMessage(LogLevel::INFO, "巡航中…… 系统正常，高度为：" + std::to_string(currentAltitude) + "米");
    }
};

// main
int main() {
    logMessage(LogLevel::INFO, "系统初始化……");
    
    // 加载配置文件
    FlightConfig myConfig = loadConfig();

    // 创建飞行控制器对象，并启动飞控
    FlightController drone;

    // 执行飞行任务
    drone.runMission(myConfig.altitude);
    
    return 0;
}