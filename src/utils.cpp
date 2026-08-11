#include <utils.h>
#include <iostream>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <sys/stat.h> // 创建目录

// 创建日志log目录
void ensureLogDirectory() {
    mkdir("log", 0777);
}

// 日志风险等级划分的辅助函数
std::string levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:   return "INFO";
        case LogLevel::WARNING:   return "WARNING";
        case LogLevel::ERROR:   return "ERROR";
        default: return "UNKNOWN";
    }
}

// 记录日志函数
void logMessage(LogLevel level, const std::string& process_name, const std::string& file, int line, const std::string& message) {
    // 检查日志目录是否存在
    ensureLogDirectory();

    // 获取当前时间
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm* local_time = std::localtime(&in_time_t);

    // 拼接日志内容
    char dateBuf[9] = {0};
    std::strftime(dateBuf, sizeof(dateBuf), "%Y%m%d", local_time);
    std::string dateStr(dateBuf);

    // 文件名
    std::string filename = "log/" + process_name + "_" + dateStr + ".log";

    // 格式化时间字符串
    char timeBuf[20] = {0};
    std::strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", local_time);

    // 提取短文件名（去掉长路径）
    std::string short_file = file;
    size_t pos = file.find_last_of("/\\");
    if (pos != std::string::npos) {
        short_file = file.substr(pos + 1);
    }

    // 拼接日志内容
    // 格式：[时间][级别][文件:行号] 内容
    std::string logEntry = "[" + std::string(timeBuf) + "]" 
                         + "[" + levelToString(level) + "]" 
                         + "[" + short_file + ":" + std::to_string(line) + "] " 
                         + message;
    
    // 写入文件
    std::ofstream logfile(filename, std::ios::app);
    if (logfile.is_open()) {
        logfile << logEntry << std::endl;
        logfile.close();
    }

    // 控制台输出 (DEBUG 级别通常只在开发时看，ERROR 需要高亮)
    if (level == LogLevel::ERROR) {
        std::cerr << logEntry << std::endl;
    } else {
        std::cout << logEntry << std::endl;
    }
}