#include <utils.h>
#include <iostream>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>

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