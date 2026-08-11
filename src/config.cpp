#include <config.h>
#include <utils.h>
#include <fstream>
#include <json.hpp>
#include <cstdlib>

using json = nlohmann::json;

Config ConfigLoader::load(const std::string& filepath) {
    Config cfg;

    cfg.heartbeat_interval = 1;
    cfg.heartbeat_timeout = 5;
    cfg.server_port = 9000;

    std::ifstream file(filepath);

    // 如果文件不存在
    if (!file.is_open()) {
        LOG_ERROR("配置文件打开失败: " + filepath);
        std::exit(1);
    }

    try {
        json j;
        file >> j;

        // 解析各个字段内容
        if (j.contains("server_ip")) cfg.server_ip = j["server_ip"];
        else LOG_WARN("缺少字段 server_ip");

        if (j.contains("server_port")) cfg.server_port = j["server_port"];
        else LOG_WARN("缺少字段 server_port");

        if (j.contains("comm_process_path")) cfg.comm_process_path = j["comm_process_path"];
        else LOG_WARN("缺少字段 comm_process_path");
        
        if (j.contains("control_process_path")) cfg.control_process_path = j["control_process_path"];
        else LOG_WARN("缺少字段 control_process_path");
        
        if (j.contains("log_level")) cfg.log_level = j["log_level"];
        else LOG_WARN("缺少字段 log_level");
        
        if (j.contains("shm_path")) cfg.shm_path = j["shm_path"];
        else LOG_WARN("缺少字段 shm_path");
        
        if (j.contains("heartbeat_timeout")) cfg.heartbeat_timeout = j["heartbeat_timeout"];
        else LOG_WARN("缺少字段 heartbeat_timeout");

        if (j.contains("heartbeat_interval")) {
            cfg.heartbeat_interval = j["heartbeat_interval"];
        } else {
            LOG_WARN("缺少字段 heartbeat_interval, 使用默认值 1");
        }    
    } catch (std::exception& e) {
        LOG_ERROR("json 解析失败: " + std::string(e.what()));
        std::exit(1);
    }

    return cfg;
}
