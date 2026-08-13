#ifndef X_PILOT_UTILS_H
#define X_PILOT_UTILS_H

#include <string>

// 日志风险等级
enum class LogLevel {
    DEBUG,         // 调试
    INFO,          // 普通
    WARNING,       // 警告
    ERROR          // 错误
};

// 核心日志函数声明
// process_name: 进程名
// file: 源码文件名 (__FILE__)
// line: 源码行号 (__LINE__)
// message: 日志内容
void logMessage(LogLevel level, const std::string& process_name, const std::string& file, int line, const std::string& message);

// 定义的LOG_DEBUG, 方便外部调用
#define LOG_DEBUG(msg)   logMessage(LogLevel::DEBUG, "x_pilot", __FILE__, __LINE__, msg)
#define LOG_INFO(msg)    logMessage(LogLevel::INFO, "x_pilot", __FILE__, __LINE__, msg)
#define LOG_WARN(msg)    logMessage(LogLevel::WARNING, "x_pilot", __FILE__, __LINE__, msg)
#define LOG_ERROR(msg)   logMessage(LogLevel::ERROR, "x_pilot", __FILE__, __LINE__, msg)

#endif
