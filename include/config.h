#pragma once
#include <string>

// 对应 config.json 的结构体
struct Config {
    std::string server_ip;
    int server_port;
    std::string comm_process_path;
    std::string control_process_path;
    std::string view_process_path;
    std::string log_level;
    std::string shm_path;
    int heartbeat_interval; // 心跳间隔
    int heartbeat_timeout;  // 心跳超时
};

namespace ConfigLoader {
    Config load(const std::string& filepath);
}